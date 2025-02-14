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

#include "octopass.h"
static pthread_mutex_t OCTOPASS_MUTEX = PTHREAD_MUTEX_INITIALIZER;

#define no_argument 0
#define required_argument 1
#define optional_argument 2
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

extern void call_getpwnam_r(const char *name);
extern void call_getpwuid_r(uid_t uid);
extern void call_pwlist(void);
extern void call_getgrnam_r(const char *name);
extern void call_getgrgid_r(gid_t gid);
extern void call_grlist(void);
extern void call_getspnam_r(const char *name);
extern void call_splist(void);

void init_mutex() {
  pthread_mutex_init(&OCTOPASS_MUTEX, NULL);
}

void help(void)
{
  printf(ANSI_COLOR_GREEN " _  ____ _ __    _____" ANSI_COLOR_RESET "\n");
  printf(ANSI_COLOR_GREEN "/ \\/  | / \\|_)/\\(__(__" ANSI_COLOR_RESET "\n");
  printf(ANSI_COLOR_GREEN "\\_/\\_ | \\_/| /--\\__)__)" ANSI_COLOR_RESET "\n");
  printf("\n");
  printf("Usage: octopass [options] [CMD]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  [USER]         get public keys from github\n");
  printf("  pam [user]     receive the password from stdin and return the auth result with the exit status\n");
  printf("  passwd [key]   displays passwd entries as octopass nss module\n");
  printf("  shadow [key]   displays shadow passwd entries as octopass nss module\n");
  printf("  group [key]    displays group entries as octopass nss module\n");
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help     show this help message and exit\n");
  printf("  -v, --version  print the version and exit\n");
  printf("\n");
}

int octopass_public_keys_unlocked(char *name)
{
  struct config con;
  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Failed to load config\n");
    return 1;
  }

  if (con.shared_users_count > 0) {
    for (int idx = 0; idx < con.shared_users_count; idx++) {
      if (strcmp(name, con.shared_users[idx]) == 0) {
        const char *members_keys = octopass_github_team_members_keys(&con);
        if (members_keys) {
          printf("%s", members_keys);
        }
        return 0;
      }
    }
  }

  const char *keys = octopass_github_user_keys(&con, name);
  if (keys) {
    printf("%s", keys);
  }

  return 0;
}

int octopass_public_keys(char *name)
{
  OCTOPASS_LOCK();
  int res = octopass_public_keys_unlocked(name);
  OCTOPASS_UNLOCK();
  return res;
}

int octopass_authentication_unlocked(int argc, char **argv)
{
  char *token = NULL;
  size_t token_size = 0;

  if (getline(&token, &token_size, stdin) == -1) {
    fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Failed to read token from stdin\n");
    free(token);
    return 2;
  }

  token[strcspn(token, "\n")] = '\0';

  char *user = NULL;
  if (argc < 3) {
    user = getenv("PAM_USER");
    if (!user) {
      fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "User is required\n");
      free(token);
      return 2;
    }
  } else {
    user = argv[2];
  }

  struct config con;
  if (octopass_config_loading(&con, OCTOPASS_CONFIG_FILE) != 0) {
    fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Failed to load config\n");
    free(token);
    return 2;
  }

  int result = octopass_autentication_with_token(&con, user, token);
  free(token);
  return result;
}

int octopass_authentication(int argc, char **argv)
{
  OCTOPASS_LOCK();
  int res = octopass_authentication_unlocked(argc, argv);
  OCTOPASS_UNLOCK();
  return res;
}

int main(int argc, char **argv)
{
  init_mutex();

  if (argc < 2 || !argv[1] || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
    help();
    return 2;
  }

  if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
    printf("%s\n", OCTOPASS_VERSION_WITH_NAME);
    return 0;
  }

  // PASSWD
  if (!strcmp(argv[1], "passwd")) {
    if (argc < 3) {
      call_pwlist();
    } else {
      char *endptr;
      long id = strtol(argv[2], &endptr, 10);

      if (*endptr == '\0' && id > 0) {
        call_getpwuid_r((uid_t)id);
      } else {
        call_getpwnam_r(argv[2]);
      }
    }
    return 0;
  }

  // GROUP
  if (!strcmp(argv[1], "group")) {
    if (argc < 3) {
      call_grlist();
    } else {
      char *endptr;
      long id = strtol(argv[2], &endptr, 10);

      if (*endptr == '\0' && id > 0) {
        call_getgrgid_r((gid_t)id);
      } else {
        call_getgrnam_r(argv[2]);
      }
    }
    return 0;
  }

  // SHADOW
  if (!strcmp(argv[1], "shadow")) {
    if (argc < 3) {
      call_splist();
    } else {
      char *endptr;
      long id = strtol(argv[2], &endptr, 10);

      if (*endptr == '\0' && id > 0) {
        fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Invalid arguments: %s\n", argv[2]);
      } else {
        call_getspnam_r(argv[2]);
      }
    }
    return 0;
  }

  // PAM
  if (!strcmp(argv[1], "pam")) {
    return octopass_authentication(argc, argv);
  }

  // PUBLIC KEYS
  //return octopass_public_keys((char *)argv[1]);
  return octopass_public_keys(argv[1]);
}
