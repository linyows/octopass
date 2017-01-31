#include "nss_octopass.h"

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
  result->pw_gecos  = "managed by nss-octopass";
  char dir[MAXBUF];
  sprintf(dir, con->home, result->pw_name);
  result->pw_dir   = strdup(dir);
  result->pw_shell = strdup(con->shell);

  return 0;
}

enum nss_status _nss_octopass_setpwent_locked(int stay_open)
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

// Called to open the passwd file
enum nss_status _nss_octopass_setpwent(int stay_open)
{
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_setpwent_locked(stay_open);
  NSS_OCTOPASS_UNLOCK();

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

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_endpwent_locked();
  NSS_OCTOPASS_UNLOCK();

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
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s", __func__);
  }
  int pack_result = pack_passwd_struct(json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen, &con);

  if (pack_result == -1) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  if (con.syslog) {
    syslog(LOG_INFO, "%s -- pw_name: %s, pw_uid: %d", __func__, result->pw_name, result->pw_uid);
  }

  ent_json_idx++;

  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in passwd file
enum nss_status _nss_octopass_getpwent_r(struct passwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwent_r_locked(result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

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
  nss_octopass_config_loading(&con, NSS_OCTOPASS_CONFIG_FILE);
  if (con.syslog) {
    syslog(LOG_INFO, "%s -- uid: %d", __func__, uid);
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
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = nss_octopass_github_team_member_by_id((int)uid, root);

  if (!data) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);

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
    syslog(LOG_INFO, "%s -- pw_name: %s, pw_uid: %d", __func__, result->pw_name, result->pw_uid);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_octopass_getpwuid_r(uid_t uid, struct passwd *result, char *buffer, size_t buflen, int *errnop)
{
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwuid_r_locked(uid, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getpwnam_r_locked(const char *name, struct passwd *result, char *buffer, size_t buflen,
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
  int status = nss_octopass_team_members(&con, &res);

  if (status != 0) {
    free(res.data);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res.data, 0, &error);
  free(res.data);
  if (!root) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = nss_octopass_github_team_member_by_name((char *)name, root);

  if (!data) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen, &con);

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
    syslog(LOG_INFO, "%s -- pw_name: %s, pw_uid: %d", __func__, result->pw_name, result->pw_uid);
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

// Find a passwd by name
enum nss_status _nss_octopass_getpwnam_r(const char *name, struct passwd *result, char *buffer, size_t buflen,
                                         int *errnop)
{
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwnam_r_locked(name, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}
