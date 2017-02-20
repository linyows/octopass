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

static int pack_shadow_struct(json_t *root, struct spwd *result, char *buffer, size_t buflen)
{
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_object(root)) {
    return -1;
  }

  json_t *j_sp_name = json_object_get(root, "login");
  if (!json_is_string(j_sp_name)) {
    return -1;
  }
  const char *login = json_string_value(j_sp_name);

  memset(buffer, '\0', buflen);

  if (bufleft <= strlen(login)) {
    return -2;
  }
  result->sp_namp = strncpy(next_buf, login, bufleft);
  next_buf += strlen(result->sp_namp) + 1;
  bufleft -= strlen(result->sp_namp) + 1;

  result->sp_pwdp   = "!!";
  result->sp_lstchg = -1;
  result->sp_min    = -1;
  result->sp_max    = -1;
  result->sp_warn   = -1;
  result->sp_inact  = -1;
  result->sp_expire = -1;
  result->sp_flag   = ~0ul;

  return 0;
}

enum nss_status _nss_octopass_setspent_locked(int stayopen)
{
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- stya_open: %d", __func__, __LINE__, stayopen);
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

  return NSS_STATUS_SUCCESS;
}

// Called to open the shadow file
enum nss_status _nss_octopass_setspent(int stayopen)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_setspent_locked(stayopen);
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_endspent_locked(void)
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

// Called to close the shadow file
enum nss_status _nss_octopass_endspent(void)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_endspent_locked();
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getspent_r_locked(struct spwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status status = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    status = _nss_octopass_setspent_locked(0);
  }

  if (status != NSS_STATUS_SUCCESS) {
    if (status == NSS_STATUS_NOTFOUND) {
      *errnop = ENOENT;
    }
    return status;
  }

  // Return notfound when there's nothing else to read.
  if (ent_json_idx >= json_array_size(ent_json_root)) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_shadow_struct(json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen);

  // A necessary input file cannot be found.
  if (pack_result == -1) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  ent_json_idx++;

  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in shadow file
enum nss_status _nss_octopass_getspent_r(struct spwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_getspent_r_locked(result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getspnam_r_locked(const char *name, struct spwd *result, char *buffer, size_t buflen,
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

  if (json_object_size(data) == 0) {
    json_decref(root);
    *errnop = ENOENT;
    if (con.syslog) {
      syslog(LOG_INFO, "%s[L%d] -- status: %s", __func__, __LINE__, "NOTFOUND");
    }
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_shadow_struct(data, result, buffer, buflen);

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
    syslog(LOG_INFO, "%s[L%d] -- status: %s, sp_namp: %s", __func__, __LINE__, "SUCCESS", result->sp_namp);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

// Find a shadow by name
enum nss_status _nss_octopass_getspnam_r(const char *name, struct spwd *result, char *buffer, size_t buflen,
                                         int *errnop)
{
  enum nss_status status;

  OCTOPASS_LOCK();
  status = _nss_octopass_getspnam_r_locked(name, result, buffer, buflen, errnop);
  OCTOPASS_UNLOCK();

  return status;
}
