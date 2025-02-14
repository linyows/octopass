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

static int pack_group_struct(json_t *root, struct group *result, char *buffer, size_t buflen, struct config *con)
{
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_array(root)) {
    return -1;
  }

  memset(buffer, '\0', buflen);

  size_t member_count = json_array_size(root);

  result->gr_mem = (char **)malloc((member_count + 1) * sizeof(char *));
  if (!result->gr_mem) {
    return -1;
  }

  result->gr_name = strdup(con->group_name);
  if (!result->gr_name) {
    free(result->gr_mem);
    return -1;
  }

  result->gr_passwd = "x";
  result->gr_gid = con->gid;

  size_t i;
  for (i = 0; i < member_count; i++) {
    json_t *j_member_obj = json_array_get(root, i);
    if (!j_member_obj) {
      continue;
    }

    json_t *j_member = json_object_get(j_member_obj, "login");
    if (!json_is_string(j_member)) {
      continue;
    }

    const char *login = json_string_value(j_member);

    if (bufleft <= strlen(login) + 1) {
      free(result->gr_name);
      free(result->gr_mem);
      return -2;
    }

    result->gr_mem[i] = strdup(login);
    if (!result->gr_mem[i]) {
      for (size_t j = 0; j < i; j++) {
        free(result->gr_mem[j]);
      }
      free(result->gr_mem);
      free(result->gr_name);
      return -1;
    }

    next_buf += strlen(result->gr_mem[i]) + 1;
    bufleft -= strlen(result->gr_mem[i]) + 1;
  }

  result->gr_mem[i] = NULL;

  return 0;
}

enum nss_status _nss_octopass_setgrent_locked(int stayopen)
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

  return NSS_STATUS_SUCCESS;
}

// Called to open the group file
enum nss_status _nss_octopass_setgrent(int stayopen)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_setgrent_locked(stayopen);
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_endgrent_locked(void)
{
  if (ent_json_root) {
    json_decref(ent_json_root);
    ent_json_root = NULL;
  }

  ent_json_root = NULL;
  ent_json_idx  = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to close the group file
enum nss_status _nss_octopass_endgrent(void)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_endgrent_locked();
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getgrent_r_locked(struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    ret = _nss_octopass_setgrent_locked(0);
    if (ret != NSS_STATUS_SUCCESS || ent_json_root == NULL) {
      *errnop = ENOENT;
      return NSS_STATUS_UNAVAIL;
    }
  }

  size_t json_size = json_array_size(ent_json_root);

  // Return notfound when there's nothing else to read.
  if (ent_json_idx >= json_size) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  json_t *json_entry = json_array_get(ent_json_root, ent_json_idx);
  if (json_entry == NULL || !json_is_array(json_entry)) {
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

  int pack_result = pack_group_struct(json_entry, result, buffer, buflen, &con);

  if (pack_result == -1) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "UNAVAIL");
    }
    return NSS_STATUS_UNAVAIL;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "TRYAGAIN");
    }
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- status: %s, gr_name: %s", __func__, __LINE__, "SUCCESS", result->gr_name);
  }

  ent_json_idx++;
  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in group file
enum nss_status _nss_octopass_getgrent_r(struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_getgrent_r_locked(result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getgrgid_r_locked(gid_t gid, struct group *result, char *buffer, size_t buflen,
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
    syslog(LOG_INFO, "%s[L%d] -- gid: %d", __func__, __LINE__, gid);
  }

  if (gid != con.gid) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
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

  if (!json_is_array(root) || json_array_size(root) == 0) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  int pack_result = pack_group_struct(root, result, buffer, buflen, &con);

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
    syslog(LOG_INFO, "%s[L%d] -- status: %s, gr_name: %s", __func__, __LINE__, "SUCCESS", result->gr_name);
  }

  status = NSS_STATUS_SUCCESS;

cleanup:
  json_decref(root);
  return status;
}

// Find a group by gid
enum nss_status _nss_octopass_getgrgid_r(gid_t gid, struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_getgrgid_r_locked(gid, result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getgrnam_r_locked(const char *name, struct group *result, char *buffer, size_t buflen,
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

  if (strcmp(name, con.group_name) != 0) {
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
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

  if (!json_is_array(root) || json_array_size(root) == 0) {
    status = NSS_STATUS_NOTFOUND;
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    goto cleanup;
  }

  int pack_result = pack_group_struct(root, result, buffer, buflen, &con);

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
    syslog(LOG_INFO, "%s[L%d] -- status: %s, gr_name: %s", __func__, __LINE__, "SUCCESS", result->gr_name);
  }

  status = NSS_STATUS_SUCCESS;

cleanup:
  json_decref(root);
  return status;
}

// Find a group by name
enum nss_status _nss_octopass_getgrnam_r(const char *name, struct group *result, char *buffer, size_t buflen,
                                         int *errnop)
{
  enum nss_status ret;

  OCTOPASS_LOCK();
  ret = _nss_octopass_getgrnam_r_locked(name, result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return ret;
}
