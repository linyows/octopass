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

  size_t team_count = json_array_size(root);

  result->gr_mem = (char **)malloc((team_count + 1) * sizeof(char *));
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

  size_t gr_mem_index = 0;

  for (size_t i = 0; i < team_count; i++) {
    json_t *j_team_obj = json_array_get(root, i);
    if (!j_team_obj) {
      continue;
    }

    json_t *j_team_id = json_object_get(j_team_obj, "id");
    if (!json_is_integer(j_team_id)) {
      continue;
    }

    json_error_t error;
    struct response res;
    int team_id = json_integer_value(j_team_id);
    int status = octopass_team_members_by_team_id(con, team_id, &res);
    if (status != 0) {
      free(res.data);
      continue;
    }

    json_t *members_root = NULL;
    members_root = json_loads(res.data, 0, &error);
    free(res.data);
    res.data = NULL;

    if (!members_root || !json_is_array(members_root)) {
      json_decref(members_root);
      continue;
    }

    for (size_t mi = 0; mi < json_array_size(members_root); mi++) {
      json_t *j_member = json_object_get(json_array_get(members_root, mi), "login");
      if (!json_is_string(j_member)) {
        continue;
      }
      const char *login = json_string_value(j_member);
      size_t login_len = strlen(login);
      if (bufleft <= strlen(login)) {
        continue;
      }
      result->gr_mem[gr_mem_index] = strdup(login);

      next_buf += login_len + 1;
      bufleft -= login_len + 1;

      gr_mem_index++;
    }
    json_decref(members_root);
  }

  result->gr_mem[gr_mem_index] = NULL;

  return 0;
}

enum nss_status _nss_octopass_setgrent_locked(int stayopen)
{
  struct config con;
  //struct response res;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d] -- stayopen: %d", __func__, __LINE__, stayopen);
  }

  json_t *root = octopass_teams(&con);
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

  struct config con;
  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    *errnop = EIO;
    return NSS_STATUS_UNAVAIL;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s[L%d]", __func__, __LINE__);
  }

  int pack_result = pack_group_struct(ent_json_root, result, buffer, buflen, &con);

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
  enum nss_status status = NSS_STATUS_UNAVAIL;
  struct config con;

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

  json_t *root = octopass_teams(&con);
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
  enum nss_status status = NSS_STATUS_UNAVAIL;
  struct config con;

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

  json_t *root = octopass_teams(&con);
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
