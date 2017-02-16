#include <getopt.h>
#include "nss_octopass.c"

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
  printf("Usage: octopass [options] [ARG]\n");
  printf("\n");
  printf("SSHD:\n");
  printf("  -- [USER]        get public keys from github\n");
  printf("\n");
  printf("PAM:\n");
  printf("  --pam [USER]     receive the password from stdin and return the auth result with the exit status)\n");
  printf("\n");
  printf("NSS:\n");
  printf("  --passwd         show like /etc/passwds, call setpwent(3), getpwent(3), endpwent(3)\n");
  printf("  --pwnam [USER]   get a passwd by name, call getpwnam(3)\n");
  printf("  --pwuid [UID]    get a passwd by uid, call getgpwuid(3)\n");
  printf("  --group          get groups, call setgrent(3), getgrent(3), endgrent(3)\n");
  printf("  --grnam [GROUP]  get a group by name, call getgrnam(3)\n");
  printf("  --grgid [GID]    get a group by gid, call getgpwuid(3)\n");
  printf("  --shadow         get shadows, call setspent(3), getspent(3), endspent(3)\n");
  printf("  --spnam [USER]   get a shadow by name, call getspnam(3)\n");
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help       show this help message and exit\n");
  printf("  -v, --version    print the version and exit\n");
  printf("\n");
}

int main(int argc, char **argv)
{
  int option_index         = 0;
  struct option longopts[] = {
      {"pam", no_argument, 0, "a"},       {"passwd", no_argument, 0, "p"},  {"pwnam", required_argument, 0, 0},
      {"pwuid", required_argument, 0, 0}, {"group", no_argument, 0, "g"},   {"grnam", required_argument, 0, 0},
      {"grgid", required_argument, 0, 0}, {"shadow", no_argument, 0, "s"},  {"spnam", required_argument, 0, 0},
      {"help", no_argument, 0, "h"},      {"version", no_argument, 0, "v"}, {0, 0, 0, 0},
  };

  int c = getopt_long(argc, argv, "ap0:1:g2:3:s4:hv", longopts, &option_index);
  if (c == -1) {
    help();
    return 2;
  }

  if (c == 0) {
    help();
    return 2;
  }

  switch (c) {
  case 'a':
    break;

  case 'p':
    call_pwlist();
    break;

  case '0':
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a username" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getpwnam_r(name);
    break;

  case '1':
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a user id" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    uid_t uid = (uid_t)atol(argv[2]);
    call_getpwuid_r(uid);
    break;

  case 'g':
    call_grlist();
    break;

  case '2':
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a gruop name" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getgrnam_r(name);
    break;

  case '3':
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a group id" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    gid_t gid = (gid_t)atol(argv[2]);
    call_getgrgid_r(gid);
    break;

  case 's':
    call_splist();
    break;

  case '4':
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a shadow name" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getspnam_r(name);
    break;

  case 'h':
    help();
    break;

  case 'v':
    printf("%s\n", NSS_OCTOPASS_VERSION_WITH_NAME);
    break;

  default:
    fprintf(stderr, ANSI_COLOR_RED "Error: Invalid command: %s" ANSI_COLOR_RESET "\n", argv[1]);
    return 1;
  }

  return 0;
}
