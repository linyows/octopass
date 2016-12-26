#ifndef NSS_OCTOPASS_H
#define NSS_OCTOPASS_H

#include <curl/curl.h>
#include <errno.h>
#include <grp.h>
#include <jansson.h>
#include <nss.h>
#include <pthread.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ucl.h>

#define NSS_OCTOPASS_LOCK() do { pthread_mutex_lock(&NSS_OCTOPASS_MUTEX); } while (0)
#define NSS_OCTOPASS_UNLOCK() do { pthread_mutex_unlock(&NSS_OCTOPASS_MUTEX); } while (0)
#ifndef NSS_OCTOPASS_SCRIPT
#define NSS_OCTOPASS_SCRIPT "/sbin/nss-octopass"
#endif

#define NSS_OCTOPASS_SERVER "api.github.com"
#define NSS_OCTOPASS_PORT "443"
#define NSS_OCTOPASS_INITIAL_BUFFER_SIZE (256 * 1024)  /* 256 KB */
#define NSS_OCTOPASS_MAX_BUFFER_SIZE (10 * 1024 * 1024)  /* 10 MB */

extern char *nss_octopass_request(const char *);
extern size_t j_strlen(json_t *);

#endif /* NSS_OCTOPASS_H */
