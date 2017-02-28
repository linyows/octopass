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
#include <regex.h>

#define OCTOPASS_VERSION "0.3.2"
#define OCTOPASS_VERSION_WITH_NAME "octopass/" OCTOPASS_VERSION
#ifndef OCTOPASS_CONFIG_FILE
#define OCTOPASS_CONFIG_FILE "/etc/octopass.conf"
#endif
#define OCTOPASS_CACHE_DIR "/var/cache/octopass"
#define OCTOPASS_LOCK()                                                                                                \
  do {                                                                                                                 \
    pthread_mutex_lock(&OCTOPASS_MUTEX);                                                                               \
  } while (0);
#define OCTOPASS_UNLOCK()                                                                                              \
  do {                                                                                                                 \
    pthread_mutex_unlock(&OCTOPASS_MUTEX);                                                                             \
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
  char **shared_users;
  int shared_users_count;
};

extern int octopass_team_members(struct config *con, struct response *res);
extern void octopass_config_loading(struct config *con, char *filename);
extern json_t *octopass_github_team_member_by_name(char *name, json_t *root);
extern json_t *octopass_github_team_member_by_id(int gh_id, json_t *root);
int octopass_autentication_with_token(struct config *con, char *user, char *token);
extern char *express_github_user_keys(struct config *con, char *user);

#endif /* OCTOPASS_H */
