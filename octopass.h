#ifndef OCTOPASS_H
#define OCTOPASS_H

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

#define OCTOPASS_VERSION "0.1.0"
#define OCTOPASS_VERSION_WITH_NAME "octopass/" OCTOPASS_VERSION
#ifndef OCTOPASS_CONFIG_FILE
#define OCTOPASS_CONFIG_FILE "/etc/octopass.conf"
#endif
#define OCTOPASS_CACHE_DIR "/var/cache/octopass"
#define OCTOPASS_LOCK()                                                                                            \
  do {                                                                                                                 \
    pthread_mutex_lock(&OCTOPASS_MUTEX);                                                                           \
  } while (0);
#define OCTOPASS_UNLOCK()                                                                                          \
  do {                                                                                                                 \
    pthread_mutex_unlock(&OCTOPASS_MUTEX);                                                                         \
  } while (0);

// 10MB
#define OCTOPASS_MAX_BUFFER_SIZE (10 * 1024 * 1024)

#define MAXBUF 1024
#define DELIM " = "

struct response {
  char *data;
  size_t size;
  long *httpstatus;
};

struct config {
  char endpoint[MAXBUF];
  char token[MAXBUF];
  char organization[MAXBUF];
  char team[MAXBUF];
  char group_name[MAXBUF];
  char home[MAXBUF];
  char shell[MAXBUF];
  long uid_starts;
  long gid;
  long cache;
  bool syslog;
};

extern int nss_octopass_team_members(struct config *con, struct response *res);
extern void nss_octopass_config_loading(struct config *con, char *filename);
extern json_t *nss_octopass_github_team_member_by_name(char *name, json_t *root);
extern json_t *nss_octopass_github_team_member_by_id(int gh_id, json_t *root);
int octopass_autentication_with_token(struct config *con, char *user, char *token);
extern char *express_github_user_keys(struct config *con, char *user);

#endif /* OCTOPASS_H */
