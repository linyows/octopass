#include "nss_octopass.h"

static pthread_mutex_t NSS_OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;

static int pack_passwd_struct(json_t *root,
                              struct passwd *result,
                              char *buffer,
                              size_t buflen) {
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_object(root)) {
    return -1;
  }

  json_t *j_pw_name = json_object_get(root, "login");
  if (!json_is_string(j_pw_name)) {
    return -1;
  }

  json_int_t j_pw_id = json_object_get(root, "id");
  if (!json_is_integer(j_pw_id)) {
    return -1;
  }

  memset(buffer, '\0', buflen);

  if (bufleft <= j_strlen(j_pw_name)) {
    return -2;
  }
  result->pw_name = strncpy(next_buf, json_string_value(j_pw_name), bufleft);
  next_buf += strlen(result->pw_name) + 1;
  bufleft  -= strlen(result->pw_name) + 1;

  result->pw_passwd = "x";
  result->pw_uid = con->uid_starts + j_pw_id;
  result->pw_gid = con->gid;
  result->pw_gecos = "managed by nss-octopass";
  char *dir[512]
  sprintf(dir, con->home, result->pw_name);
  result->pw_dir = dir;
  result->pw_shell = con->shell;

  return 0;
}

// Called to open the passwd file
enum nss_status _nss_octopass_setpwent(int stay_open) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_setpwent_locked(stay_open);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_setpwent_locked(int stay_open) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res;
  int status = nss_octopass_team_members(&con, res);

  if (status != 0) {
    free(res);
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res, 0, &error);
  free(res);

  if (!root) {
    return NSS_STATUS_UNAVAIL;
  }

  if (!json_is_array(root)) {
    json_decref(root);
    return NSS_STATUS_UNAVAIL;
  }

  ent_json_root = root;
  ent_json_idx = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to close the passwd file
enum nss_status _nss_octopass_endpwent(void) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_endpwent_locked();
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_endpwent_locked(void) {
  if (ent_json_root){
    while (ent_json_root->refcount > 0) json_decref(ent_json_root);
  }
  ent_json_root = NULL;
  ent_json_idx = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in passwd file
enum nss_status _nss_octopass_getpwent_r(struct passwd *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwent_r_locked(result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getpwent_r_locked(struct passwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  enum nss_status ret = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    ret = _nss_octopass_setpwent_locked(0);
  }

  if (ret != NSS_STATUS_SUCCESS) return ret;

  int pack_result = pack_passwd_struct(
    json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen
  );

  if (pack_result == -1) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  if (pack_result == -2) {
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  // Return notfound when there's nothing else to read.
  if (ent_json_idx >= json_array_size(ent_json_root)) {
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
  }

  ent_json_idx++;

  return NSS_STATUS_SUCCESS;
}

// Find a passwd by uid
enum nss_status _nss_octopass_getpwuid_r_locked(uid_t uid,
                                                struct passwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res_body;
  int status = nss_octopass_team_members(&con, res_body);

  if (status != 0) {
    free(res_body);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res_body, 0, &error);
  free(res_body);
  if (!root) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = nss_octopass_github_team_member_by_id((int)uid, root);

  if (!data) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_octopass_getpwuid_r(uid_t uid,
                                         struct passwd *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwuid_r_locked(uid, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

// Find a passwd by name
enum nss_status _nss_octopass_getpwnam_r(const char *name,
                                         struct passwd *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getpwnam_r_locked(name, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getpwnam_r_locked(const char *name,
                                                struct passwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res_body;
  int status = nss_octopass_team_members(&con, res_body);

  if (status != 0) {
    free(res_body);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res_body, 0, &error);
  free(res_body);
  if (!root) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  json_t *data = nss_octopass_github_team_member_by_name(name, root);

  if (!data) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  int pack_result = pack_passwd_struct(data, result, buffer, buflen);

  if (pack_result == -1) {
    json_decref(root);
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  if (pack_result == -2) {
    json_decref(root);
    *errnop = ERANGE;
    return NSS_STATUS_TRYAGAIN;
  }

  json_decref(root);
  return NSS_STATUS_SUCCESS;
}
