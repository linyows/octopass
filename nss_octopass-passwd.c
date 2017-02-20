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

  if (bufleft <= strlen(login)) {
    return -2;
  }

  result->pw_name = strncpy(next_buf, login, bufleft);
  next_buf += strlen(result->pw_name) + 1;
  bufleft -= strlen(result->pw_name) + 1;

  result->pw_passwd = "x";
  result->pw_uid    = con->uid_starts + id;
  result->pw_gid    = con->gid;
  result->pw_gecos  = "managed by octopass";
  char dir[MAXBUF];
  sprintf(dir, con->home, result->pw_name);
  result->pw_dir   = strdup(dir);
  result->pw_shell = strdup(con->shell);

  return 0;
}

enum nss_status _nss_octopass_setpwent_locked(int stayopen)
{
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- stayopen: %d", __func__, __LINE__, stayopen);
  }
  int status = octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);

  if (!root) {
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  if (!json_is_array(root)) {
    json_decref(root);
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
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
    while (ent_json_root->refcount > 0) {
      json_decref(ent_json_root);
    }
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
  }

  if (ret != NSS_STATUS_SUCCESS) {
    return ret;
  }

  // Return notfound when there's nothing else to read.
  if (ent_json_idx >= json_array_size(ent_json_root)) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  struct config con;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d]", __func__, __LINE__);
  }
  int pack_result = pack_passwd_struct(json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen, &con);

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
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d", __func__, __LINE__, "SUCCESS", result->pw_name,
           result->pw_uid);
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
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- uid: %d", __func__, __LINE__, uid);
  }
  int status = octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  if (!root) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  int gh_id = uid - con.uid_starts;

  json_t *data = octopass_github_team_member_by_id(gh_id, root);

  if (json_object_size(data) == 0) {
    json_decref(root);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d", __func__, __LINE__, "SUCCESS", result->pw_name,
           result->pw_uid);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
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
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- name: %s", __func__, __LINE__, name);
  }
  int status = octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  if (!root) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = octopass_github_team_member_by_name((char *)name, root);

  if (!data) {
    json_decref(root);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "TRYAGAIN");
    }
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, pw_name: %s, pw_uid: %d", __func__, __LINE__, "SUCCESS", result->pw_name,
           result->pw_uid);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
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
