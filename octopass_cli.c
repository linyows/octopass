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

#include "octopass.c"
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
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);

  if (con.shared_users_count > 0) {
    int idx;
    for (idx = 0; idx < con.shared_users_count; idx++) {
      if (strcmp(name, con.shared_users[idx]) == 0) {
        const char *members_keys = octopass_github_team_members_keys(&con);
        if (members_keys != NULL) {
          printf("%s", members_keys);
        }
        return 0;
      }
    }
  }

  const char *keys = octopass_github_user_keys(&con, name);
  if (keys != NULL) {
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
  char token[40 + 1];
  fgets(token, sizeof(token), stdin);
  if (token == NULL) {
    fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Token is required\n");
    return 2;
  }

  char *user;
  if (argc < 3) {
    user = getenv("PAM_USER");
    if (user == NULL) {
      fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "User is required\n");
      return 2;
    }
  } else {
    user = argv[2];
  }

  struct config con;
  octopass_config_loading(&con, OCTOPASS_CONFIG_FILE);
  return octopass_autentication_with_token(&con, user, token);
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
  if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
    help();
    return 2;
  }

  if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
    printf("%s\n", OCTOPASS_VERSION_WITH_NAME);
    return 0;
  }

  // PASSWD
  if (strcmp(argv[1], "passwd") == 0) {
    if (argc < 3) {
      call_pwlist();
    } else {
      long id = atol(argv[2]);
      if (id > 0) {
        uid_t uid = (uid_t)atol(argv[2]);
        call_getpwuid_r(uid);
      } else {
        call_getpwnam_r((char *)argv[2]);
      }
    }
    return 0;
  }

  // GROUP
  if (strcmp(argv[1], "group") == 0) {
    if (argc < 3) {
      call_grlist();
    } else {
      long id = atol(argv[2]);
      if (id > 0) {
        gid_t gid = (gid_t)atol(argv[2]);
        call_getgrgid_r(gid);
      } else {
        call_getgrnam_r((char *)argv[2]);
      }
    }
    return 0;
  }

  // SHADOW
  if (strcmp(argv[1], "shadow") == 0) {
    if (argc < 3) {
      call_splist();
    } else {
      long id = atol(argv[2]);
      if (id > 0) {
        fprintf(stderr, ANSI_COLOR_RED "Error: " ANSI_COLOR_RESET "Invalid arguments: %s\n", argv[2]);
      } else {
        call_getspnam_r((char *)argv[2]);
      }
    }
    return 0;
  }

  // PAM
  if (strcmp(argv[1], "pam") == 0) {
    return octopass_authentication(argc, argv);
  }

  // PUBLIC KEYS
  return octopass_public_keys((char *)argv[1]);
}
