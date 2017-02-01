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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NSS_OCTOPASS_VERSION "0.1.0"
#define NSS_OCTOPASS_VERSION_WITH_NAME "nss-octopass/" NSS_OCTOPASS_VERSION
#ifndef NSS_OCTOPASS_CONFIG_FILE
#define NSS_OCTOPASS_CONFIG_FILE "/etc/octopass.conf"
#endif
#define NSS_OCTOPASS_CACHE_DIR "/tmp"
#define NSS_OCTOPASS_LOCK()                                                                                            \
  do {                                                                                                                 \
    pthread_mutex_lock(&NSS_OCTOPASS_MUTEX);                                                                           \
  } while (0);
#define NSS_OCTOPASS_UNLOCK()                                                                                          \
  do {                                                                                                                 \
    pthread_mutex_unlock(&NSS_OCTOPASS_MUTEX);                                                                         \
  } while (0);
#ifndef NSS_OCTOPASS_SCRIPT
#define NSS_OCTOPASS_SCRIPT "/sbin/nss-octopass"
#endif

// 256KB
#define NSS_OCTOPASS_INITIAL_BUFFER_SIZE (256 * 1024)
// 10MB
#define NSS_OCTOPASS_MAX_BUFFER_SIZE (10 * 1024 * 1024)

#define MAXBUF 1024
#define DELIM " = "

struct response {
  char *data;
  size_t size;
};

struct config {
  char endpoint[MAXBUF];
  char token[MAXBUF];
  char organization[MAXBUF];
  char team[MAXBUF];
  char group_name[MAXBUF];
  char home[MAXBUF];
  char shell[MAXBUF];
  char gecos[MAXBUF];
  long cache;
  long timeout;
  long uid_starts;
  long gid;
  bool syslog;
};

extern int nss_octopass_team_members(struct config *con, struct response *res);
extern void nss_octopass_config_loading(struct config *con, char *filename);
extern json_t *nss_octopass_github_team_member_by_name(char *name, json_t *root);
extern json_t *nss_octopass_github_team_member_by_id(int gh_id, json_t *root);

#endif /* NSS_OCTOPASS_H */
