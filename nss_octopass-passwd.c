/* Management linux user and authentication with the organization/team on Github.
   Copyright (C) 2017 Tomohisa Oda

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "octopass.h"

static pthread_mutex_t OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root          = NULL;
static int ent_json_idx               = 0;

static int pack_passwd_struct(json_t *root, struct passwd *result, char *buffer, size_t buflen, struct config *con)
{
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_object(root)) {
    return -1;
  }

  json_t *j_pw_name = json_object_get(root, "login");
  if (!json_is_string(j_pw_name)) {
    return -1;
  }
  const char *login = json_string_value(j_pw_name);

  json_t *j_pw_uid = json_object_get(root, "id");
  if (!json_is_integer(j_pw_uid)) {
    return -1;
  }
  const json_int_t id = json_integer_value(j_pw_uid);

  memset(buffer, '\0', buflen);

  if (bufleft <= strlen(login) + 1) {
    return -2;
  }

  snprintf(next_buf, bufleft, "%s", login);
  result->pw_name = next_buf;

  next_buf += strlen(result->pw_name) + 1;
  bufleft -= strlen(result->pw_name) + 1;

  result->pw_passwd = "x";
  result->pw_uid    = con->uid_starts + id;
  result->pw_gid    = con->gid;
  result->pw_gecos  = "managed by octopass";

  char dir[MAXBUF];
  snprintf(dir, sizeof(dir), con->home, result->pw_name);
  result->pw_dir = strdup(dir);

  result->pw_shell = strdup(con->shell);

  return 0;
}

enum nss_status _nss_octopass_setpwent_locked(int stayopen)
{
  json_t *root = NULL;
  json_error_t error;

  struct config con;
  struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);

  if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- stayopen: %d", __func__, __LINE__, stayopen);
  }

  int status = octopass_members(&con, &res);
  if (status != 0 || res.data == NULL) {
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  res.data = NULL;

  if (!root) {
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  if (!json_is_array(root)) {
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    json_decref(root);
    return NSS_STATUS_UNAVAIL;
  }

  if (ent_json_root) {
    json_decref(ent_json_root);
  }

  ent_json_root = root;
  ent_json_idx  = 0;

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "SUCCESS");
  }

  return NSS_STATUS_SUCCESS;
}

// Called to open the passwd file
enum nss_status _nss_octopass_setpwent(int stayopen)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_setpwent_locked(stayopen);
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_endpwent_locked(void)
{
  if (ent_json_root) {
    json_decref(ent_json_root);
    ent_json_root = NULL;
  }

  ent_json_root = NULL;
  ent_json_idx  = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to close the passwd file
enum nss_status _nss_octopass_endpwent(void)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_endpwent_locked();
  OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getpwent_r_locked(struct passwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    ret = _nss_octopass_setpwent_locked(0);
    if (ret != NSS_STATUS_SUCCESS || ent_json_root == NULL) {
      *errnop = ENOENT;
      return NSS_STATUS_UNAVAIL;
    }
  }

  size_t json_size = json_array_size(ent_json_root);

  if (ent_json_idx >= json_size) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  json_t *json_entry = json_array_get(ent_json_root, ent_json_idx);
  if (json_entry == NULL || !json_is_object(json_entry)) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  struct config con;
  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    *errnop = EIO;
    return NSS_STATUS_UNAVAIL;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d]", __func__, __LINE__);
  }

  int pack_result = pack_passwd_struct(json_entry, result, buffer, buflen, &con);

  if (pack_result == -1) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "TRYAGAIN");
    }
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d", 
           __func__, __LINE__, "SUCCESS", result->pw_name, result->pw_uid);
  }

  ent_json_idx++;
  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in passwd file
enum nss_status _nss_octopass_getpwent_r(struct passwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_getpwent_r_locked(result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return ret;
}

// Find a passwd by uid
enum nss_status _nss_octopass_getpwuid_r_locked(uid_t uid, struct passwd *result, char *buffer, size_t buflen,
                                                int *errnop)
{
  json_t *root = NULL;
  json_error_t error;
  enum nss_status status = NSS_STATUS_UNAVAIL;

  struct config con;
  struct response res;

  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    *errnop = EIO;
    return NSS_STATUS_UNAVAIL;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- uid: %d", __func__, __LINE__, uid);
  }

  if (octopass_members(&con, &res) != 0 || res.data == NULL) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  res.data = NULL;

  if (!root) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  int gh_id = uid - con.uid_starts;
  json_t *data = octopass_github_team_member_by_id(gh_id, root);

  if (!data || json_object_size(data) == 0) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);
  if (pack_result == -1) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  if (pack_result == -2) {
    status = NSS_STATUS_TRYAGAIN;
    *errnop = ERANGE;
    goto cleanup;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d",
           __func__, __LINE__, "SUCCESS", result->pw_name, result->pw_uid);
  }

  status = NSS_STATUS_SUCCESS;

cleanup:
  json_decref(root);
  return status;
}

enum nss_status _nss_octopass_getpwuid_r(uid_t uid, struct passwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_getpwuid_r_locked(uid, result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getpwnam_r_locked(const char *name, struct passwd *result, char *buffer, size_t buflen,
                                                int *errnop)
{
  json_t *root = NULL;
  json_error_t error;
  enum nss_status status = NSS_STATUS_UNAVAIL;

  struct config con;
  struct response res;

  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    *errnop = EIO;
    return NSS_STATUS_UNAVAIL;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- name: %s", __func__, __LINE__, name);
  }

  if (octopass_members(&con, &res) != 0 || res.data == NULL) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  res.data = NULL;

  if (!root) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = octopass_github_team_member_by_name((char *)name, root);
  if (!data || json_object_size(data) == 0) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);
  if (pack_result == -1) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  if (pack_result == -2) {
    status = NSS_STATUS_TRYAGAIN;
    *errnop = ERANGE;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "TRYAGAIN");
    }
    goto cleanup;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d",
           __func__, __LINE__, "SUCCESS", result->pw_name, result->pw_uid);
  }

  status = NSS_STATUS_SUCCESS;

cleanup:
  json_decref(root);
  return status;
}

// Find a passwd by name
enum nss_status _nss_octopass_getpwnam_r(const char *name, struct passwd *result, char *buffer, size_t buflen,
                                         int *errnop)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_getpwnam_r_locked(name, result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return ret;
}
