#include "nss_octopass.c"

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
  printf("Usage: nss-octopass [CMD] [KEY]\n");
  printf("\n");
  printf("Command:\n");
  printf("  passwd             get passwds, call setpwent(3), getpwent(3), endpwent(3)\n");
  printf("  getpwnam [NAME]    get a passwd by name, call getpwnam(3)\n");
  printf("  getpwuid [UID]     get a passwd by uid, call getgpwuid(3)\n");
  printf("  group              get groups, call setgrent(3), getgrent(3), endgrent(3)\n");
  printf("  getgrnam [NAME]    get a group by name, call getgrnam(3)\n");
  printf("  getgrgid [GID]     get a group by gid, call getgpwuid(3)\n");
  printf("  shadow             get shadows, call setspent(3), getspent(3), endspent(3)\n");
  printf("  getspnam [NAME]    get a shadow by name, call getspnam(3)\n");
  printf("\n");
}

int main(int argc, char **argv)
{
  if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
    help();
    return 2;
  }

  // passwd
  if (strcmp(argv[1], "passwd") == 0) {
    call_pwlist();
    return 0;
  }

  if (strcmp(argv[1], "getpwnam") == 0) {
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a username" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getpwnam_r(name);
    return 0;
  }

  if (strcmp(argv[1], "getpwuid") == 0) {
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a user id" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    uid_t uid = (uid_t)atol(argv[2]);
    call_getpwuid_r(uid);
    return 0;
  }

  // group
  if (strcmp(argv[1], "group") == 0) {
    call_grlist();
    return 0;
  }

  if (strcmp(argv[1], "getgrnam") == 0) {
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a gruop name" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getgrnam_r(name);
    return 0;
  }

  if (strcmp(argv[1], "getgrgid") == 0) {
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a group id" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    gid_t gid = (gid_t)atol(argv[2]);
    call_getgrgid_r(gid);
    return 0;
  }

  // shadow
  if (strcmp(argv[1], "shadow") == 0) {
    call_splist();
    return 0;
  }

  if (strcmp(argv[1], "getspnam") == 0) {
    if (argc < 3) {
      fprintf(stderr, ANSI_COLOR_RED "Error: %s requires a shadow name" ANSI_COLOR_RESET "\n", argv[1]);
      return 1;
    }
    const char *name = (char *)argv[2];
    call_getspnam_r(name);
    return 0;
  }

  fprintf(stderr, ANSI_COLOR_RED "Error: Invalid command: %s" ANSI_COLOR_RESET "\n", argv[1]);
  return 1;
}
