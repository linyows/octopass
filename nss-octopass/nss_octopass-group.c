#include "nss_octopass.h"

static pthread_mutex_t NSS_OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;

static int pack_group_struct(json_t *root,
                             struct group *result,
                             char *buffer,
                             size_t buflen) {
  char *next_buf = buffer;
  size_t bufleft = buflen;

  if (!json_is_object(root)) {
    return -1;
  }

  json_t *j_member;
  json_t *j_gr_mem = json_object_get(root, "gr_mem");

  if (!json_is_array(j_gr_mem)) return -1;

  memset(buffer, '\0', buflen);

  // Carve off some space for array of members.
  result->gr_mem = (char **)next_buf;
  next_buf += (json_array_size(j_gr_mem) + 1) * sizeof(char *);
  bufleft  -= (json_array_size(j_gr_mem) + 1) * sizeof(char *);

  for (int i = 0; i < json_array_size(j_gr_mem); i++) {
    j_member = json_array_get(j_gr_mem, i);
    if (!json_is_string(j_member)) {
      return -1;
    }

    if (bufleft <= j_strlen(j_member)) {
      return -2;
    }
    strncpy(next_buf, json_string_value(j_member), bufleft);
    result->gr_mem[i] = next_buf;

    next_buf += strlen(result->gr_mem[i]) + 1;
    bufleft  -= strlen(result->gr_mem[i]) + 1;
  }

  result->gr_name = con->group_name;
  result->gr_passwd = "x";
  result->gr_gid = con->gid;

  return 0;
}

// Called to open the group file
enum nss_status _nss_octopass_setgrent(int stayopen) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_setgrent_locked(stayopen);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_setgrent_locked(int stayopen) {
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

// Called to close the group file
enum nss_status _nss_octopass_endgrent(void) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_endgrent_locked();
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_endgrent_locked(void) {
  if (ent_json_root) {
    while (ent_json_root->refcount > 0) json_decref(ent_json_root);
  }

  ent_json_root = NULL;
  ent_json_idx = 0;

  return NSS_STATUS_SUCCESS;
}

// Called to look up next entry in group file
enum nss_status _nss_octopass_getgrent_r(struct group *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status status;

  NSS_OCTOPASS_LOCK();
  status = _nss_octopass_getgrent_r_locked(result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return status;
}

enum nss_status _nss_octopass_getgrent_r_locked(struct group *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  enum nss_status ret = NSS_STATUS_SUCCESS;

  if (ent_json_root == NULL) {
    ret = _nss_octopass_setgrent_locked(0);
  }

  if (ret != NSS_STATUS_SUCCESS) return ret;

  int pack_result = pack_group_struct(
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

// Find a group by gid
enum nss_status _nss_octopass_getgrgid_r(gid_t gid,
                                         struct group *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getgrgid_r_locked(gid, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getgrgid_r_locked(gid_t gid,
                                                struct group *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res;
  int status = nss_octopass_team_members(&con, res);

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

  int pack_result = pack_group_struct(root, result, buffer, buflen);

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

// Find a group by name
enum nss_status _nss_octopass_getgrnam_r(const char *name,
                                         struct group *result,
                                         char *buffer,
                                         size_t buflen,
                                         int *errnop) {
  enum nss_status ret;

  NSS_OCTOPASS_LOCK();
  ret = _nss_octopass_getgrnam_r_locked(name, result, buffer, buflen, errnop);
  NSS_OCTOPASS_UNLOCK();

  return ret;
}

enum nss_status _nss_octopass_getgrnam_r_locked(const char *name,
                                                struct group *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
  json_t *root;
  json_error_t error;

  struct config con;
  char *res;
  int status = nss_octopass_team_members(&con, res);

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

  int pack_result = pack_group_struct(root, result, buffer, buflen);

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
