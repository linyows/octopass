#include "nss_octopass.h"

static pthread_mutex_t NSS_OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;

static int pack_shadow_struct(json_t *root,
                              struct spwd *result,
                              char *buffer,
                              size_t buflen) {
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_object(root)) {
    return -1;
  }

  json_t *j_sp_namp = json_object_get(root, "login");
  if (!json_is_string(j_sp_namp)) {
    return -1;
  }

  memset(buffer, '\0', buflen);

  if (bufleft <= j_strlen(j_sp_namp)) {
    return -2;
  }
  result->sp_namp = strncpy(next_buf, json_string_value(j_sp_namp), bufleft);
  next_buf += strlen(result->sp_namp) + 1;
  bufleft -= strlen(result->sp_namp) + 1;

  result->sp_pwdp = "*";
  result->sp_lstchg = 13571;
  result->sp_min = 0;
  result->sp_max = 99999;
  result->sp_warn = 7;
  result->sp_inact = -1;
  result->sp_expire = -1;
  result->sp_flag = ~0ul;

  return 0;
}

// Called to open the shadow file
enum nss_status _nss_octopass_setspent(int stayopen) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_setspent_locked(stayopen);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_setspent_locked(int stayopen) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res;
  int status = nss_octopass_request(&con, res);
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

enum nss_status _nss_octopass_endspent_locked(void) {
  if (ent_json_root){
      while (ent_json_root->refcount > 0) json_decref(ent_json_root);
  }

  ent_json_root = NULL;
  ent_json_idx = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to close the shadow file
enum nss_status _nss_octopass_endspent(void) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_endspent_locked();
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getspent_r_locked(struct spwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  enum nss_status status = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    status = _nss_octopass_setspent_locked(0);
  }

  if (status != NSS_STATUS_SUCCESS) {
    return status;
  }

  int pack_result = pack_shadow_struct(
    json_array_get(ent_json_root, ent_json_idx), result, buffer, buflen
  );

  // A necessary input file cannot be found.
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

// Called to look up next entry in shadow file
enum nss_status _nss_octopass_getspent_r(struct spwd *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_getspent_r_locked(result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

// Find a shadow by name
enum nss_status _nss_octopass_getspnam_r(const char *name,
                                         struct spwd *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_getspnam_r_locked(name, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getspnam_r_locked(const char *name,
                                                struct spwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res;
  int status = nss_octopass_request(&con, res);
  if (status != 0) {
      free(res);
      *errnop = ENOENT;
      return NSS_STATUS_UNAVAIL;
  }

  root = json_loads(res, 0, &error);
  free(res);

  if (!root) {
    *errnop = ENOENT;
    return NSS_STATUS_UNAVAIL;
  }

  int pack_result = pack_shadow_struct(root, result, buffer, buflen);
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
