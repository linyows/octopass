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
  printf("Usage: octopass [options] [CMD]\n");
  printf("\n");
  printf("Commands:\n");
  printf("  [USER]               get public keys from github\n");
  printf("  pam [user]           receive the password from stdin and return the auth result with the exit status)\n");
  printf("  getent passwd [key]  displays passwd entries as octopass nss module\n");
  printf("  getent shadow [key]  displays shadow passwd entries as octopass nss module\n");
  printf("  getent group [key]   displays group entries as octopass nss module\n");
  printf("\n");
  printf("Options:\n");
  printf("  -h, --help           show this help message and exit\n");
  printf("  -v, --version        print the version and exit\n");
  printf("\n");
}

int main(int argc, char **argv)
{
  if (argc < 1 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
    help();
    return 2;
  }

  if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
    printf("%s\n", OCTOPASS_VERSION_WITH_NAME);
    return 0;
  }

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

  if (strcmp(argv[1], "shadow") == 0) {
    if (argc < 3) {
      call_splist();
    } else {
      long id = atol(argv[2]);
      if (id > 0) {
        fprintf(stderr, ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET " Invalid arguments: %s\n", argv[2]);
      } else {
        call_getspnam_r((char *)argv[2]);
      }
    }
    return 0;
  }

  if (strcmp(argv[1], "pam") == 0) {
    return 0;
  }

  fprintf(stderr, ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET " Invalid command: %s\n", argv[1]);
  return 0;
}
