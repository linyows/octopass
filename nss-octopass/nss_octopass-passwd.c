#include "nss_octopass.h"

static pthread_mutex_t NSS_OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;
static json_t *ent_json_root = NULL;
static int ent_json_idx = 0;

// -1 Failed to parse
// -2 Buffer too small
static int pack_passwd_struct(json_t *root,
                              struct passwd *result,
                              char *buffer,
                              size_t buflen) {

    char *next_buf = buffer;
    size_t bufleft = buflen;

    if (!json_is_object(root)) return -1;

    json_t *j_pw_name = json_object_get(root, "pw_name");
    json_t *j_pw_passwd = json_object_get(root, "pw_passwd");
    json_t *j_pw_uid = json_object_get(root, "pw_uid");
    json_t *j_pw_gid = json_object_get(root, "pw_gid");
    json_t *j_pw_gecos = json_object_get(root, "pw_gecos");
    json_t *j_pw_dir = json_object_get(root, "pw_dir");
    json_t *j_pw_shell = json_object_get(root, "pw_shell");

    if (!json_is_string(j_pw_name)) return -1;
    if (!json_is_string(j_pw_passwd)) return -1;
    if (!json_is_integer(j_pw_uid)) return -1;
    if (!json_is_integer(j_pw_gid)) return -1;
    if (!json_is_string(j_pw_gecos) && !json_is_null(j_pw_gecos)) return -1;
    if (!json_is_string(j_pw_dir)) return -1;
    if (!json_is_string(j_pw_shell)) return -1;

    memset(buffer, '\0', buflen);

    if (bufleft <= j_strlen(j_pw_name)) return -2;
    result->pw_name = strncpy(next_buf, json_string_value(j_pw_name), bufleft);
    next_buf += strlen(result->pw_name) + 1;
    bufleft  -= strlen(result->pw_name) + 1;

    if (bufleft <= j_strlen(j_pw_passwd)) return -2;
    result->pw_passwd = strncpy(next_buf, json_string_value(j_pw_passwd), bufleft);
    next_buf += strlen(result->pw_passwd) + 1;
    bufleft  -= strlen(result->pw_passwd) + 1;

    // Yay, ints are so easy!
    result->pw_uid = json_integer_value(j_pw_uid);
    result->pw_gid = json_integer_value(j_pw_gid);

    if (json_is_null(j_pw_gecos)) {
        if (bufleft <= 1) return -2;
        result->pw_gecos = strncpy(next_buf, "", 1);
        next_buf += 1;
        bufleft -= 1;
    } else {
        if (bufleft <= j_strlen(j_pw_gecos)) return -2;
        result->pw_gecos = strncpy(next_buf, json_string_value(j_pw_gecos), bufleft);
        next_buf += strlen(result->pw_gecos) + 1;
        bufleft  -= strlen(result->pw_gecos) + 1;
    }

    if (bufleft <= j_strlen(j_pw_dir)) return -2;
    result->pw_dir = strncpy(next_buf, json_string_value(j_pw_dir), bufleft);
    next_buf += strlen(result->pw_dir) + 1;
    bufleft  -= strlen(result->pw_dir) + 1;

    if (bufleft <= j_strlen(j_pw_shell)) return -2;
    result->pw_shell = strncpy(next_buf, json_string_value(j_pw_shell), bufleft);
    next_buf += strlen(result->pw_shell) + 1;
    bufleft  -= strlen(result->pw_shell) + 1;

    return 0;
}

enum nss_status _nss_octopass_setpwent_locked(int stayopen) {
    char url[512];
    json_t *json_root;
    json_error_t json_error;

    snprintf(url, 512, "http://" NSS_OCTOPASS_SERVER ":" NSS_OCTOPASS_PORT "/passwd");

    char *response = nss_octopass_request(url);
    if (!response) {
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        return NSS_STATUS_UNAVAIL;
    }

    if (!json_is_array(json_root)) {
        json_decref(json_root);
        return NSS_STATUS_UNAVAIL;
    }

    ent_json_root = json_root;
    ent_json_idx = 0;

    return NSS_STATUS_SUCCESS;
}

// Called to open the passwd file
enum nss_status _nss_octopass_setpwent(int stayopen) {
    enum nss_status ret;
    NSS_OCTOPASS_LOCK();
    ret = _nss_octopass_setpwent_locked(stayopen);
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

// Called to close the passwd file
enum nss_status _nss_octopass_endpwent(void) {
    enum nss_status ret;
    NSS_OCTOPASS_LOCK();
    ret = _nss_octopass_endpwent_locked();
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

// Find a passwd by uid
enum nss_status _nss_octopass_getpwuid_r_locked(uid_t uid,
                                                struct passwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
    char url[512];
    json_t *json_root;
    json_error_t json_error;

    snprintf(url, 512, "http://" NSS_OCTOPASS_SERVER ":" NSS_OCTOPASS_PORT "/passwd?uid=%d", uid);

    char *response = nss_octopass_request(url);
    if (!response) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_passwd_struct(json_root, result, buffer, buflen);

    if (pack_result == -1) {
        json_decref(json_root);
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    if (pack_result == -2) {
        json_decref(json_root);
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    json_decref(json_root);

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

enum nss_status _nss_octopass_getpwnam_r_locked(const char *name,
                                                struct passwd *result,
                                                char *buffer,
                                                size_t buflen,
                                                int *errnop) {
    char url[512];
    json_t *json_root;
    json_error_t json_error;

    snprintf(url, 512, "http://" NSS_OCTOPASS_SERVER ":" NSS_OCTOPASS_PORT "/passwd?name=%s", name);

    char *response = nss_octopass_request(url);
    if (!response) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    json_root = json_loads(response, 0, &json_error);

    if (!json_root) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    int pack_result = pack_passwd_struct(json_root, result, buffer, buflen);
    if (pack_result == -1) {
        json_decref(json_root);
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    if (pack_result == -2) {
        json_decref(json_root);
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    json_decref(json_root);

    return NSS_STATUS_SUCCESS;
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
