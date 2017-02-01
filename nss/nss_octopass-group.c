#include "nss_octopass.h"

static pthread_mutex_t NSS_OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root              = NULL;
static int ent_json_idx                   = 0;

static int pack_group_struct(json_t *root, struct group *result, char *buffer, size_t buflen, struct config *con)
{
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_array(root)) {
    return -1;
  }

  memset(buffer, '\0', buflen);

  // Carve off some space for array of members.
  result->gr_mem    = (char **)next_buf;
  result->gr_name   = strdup(con->group_name);
  result->gr_passwd = "x";
  result->gr_gid    = con->gid;

  int i;
  for (i = 0; i < json_array_size(root); i++) {
    json_t *j_member = json_object_get(json_array_get(root, i), "login");
    if (!json_is_string(j_member)) {
      return -1;
    }
    const char *login = json_string_value(j_member);
    if (bufleft <= strlen(login)) {
      return -2;
    }
    result->gr_mem[i] = strdup(login);

    next_buf += strlen(result->gr_mem[i]) + 1;
    bufleft -= strlen(result->gr_mem[i]) + 1;
  }

  return 0;
}

enum nss_status _nss_octopass_setgrent_locked(int stay_open)
{
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s -- stay_open: %d", __func__, stay_open);
  }
  int status = nss_octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);

  if (!root) {
    return NSS_STATUS_UNAVAIL;
  }

  if (!json_is_array(root)) {
    json_decref(root);
    return NSS_STATUS_UNAVAIL;
  }

  ent_json_root = root;
  ent_json_idx  = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to open the group file
enum nss_status _nss_octopass_setgrent(int stay_open)
{
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_setgrent_locked(stay_open);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_endgrent_locked(void)
{
  if (ent_json_root) {
    while (ent_json_root->refcount > 0)
      json_decref(ent_json_root);
  }

  ent_json_root = NULL;
  ent_json_idx  = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to close the group file
enum nss_status _nss_octopass_endgrent(void)
{
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_endgrent_locked();
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getgrent_r_locked(struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    ret = _nss_octopass_setgrent_locked(0);
  }

  if (ret != NSS_STATUS_SUCCESS) {
    return ret;
  }

  // Return notfound when there's nothing else to read.
  if (ent_json_idx > 0) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  struct config con;
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s", __func__);
  }
  int pack_result = pack_group_struct(ent_json_root, result, buffer, buflen, &con);

  if (pack_result == -1) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s -- gr_name: %s", __func__, result->gr_name);
  }

  ent_json_idx++;

  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in group file
enum nss_status _nss_octopass_getgrent_r(struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_getgrent_r_locked(result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getgrgid_r_locked(gid_t gid, struct group *result, char *buffer, size_t buflen,
                                                int *errnop)
{
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s -- gid: %d", __func__, gid);
  }

  if (gid != con.gid) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int status = nss_octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);

  json_t *data = nss_octopass_github_team_member_by_id((int)gid, root);

  if (json_object_size(data) == 0) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_group_struct(data, result, buffer, buflen, &con);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s -- gr_name: %s", __func__, result->gr_name);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

// Find a group by gid
enum nss_status _nss_octopass_getgrgid_r(gid_t gid, struct group *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getgrgid_r_locked(gid, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getgrnam_r_locked(const char *name, struct group *result, char *buffer, size_t buflen,
                                                int *errnop)
{
  json_t *root;
  json_error_t error;

  struct config con;
  struct response res;
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s -- name: %s", __func__, name);
  }

  if (strcmp(name, con.group_name) != 0) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int status = nss_octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  if (!root) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  int pack_result = pack_group_struct(root, result, buffer, buflen, &con);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s -- gr_name: %s", __func__, result->gr_name);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

// Find a group by name
enum nss_status _nss_octopass_getgrnam_r(const char *name, struct group *result, char *buffer, size_t buflen,
                                         int *errnop)
{
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getgrnam_r_locked(name, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}
