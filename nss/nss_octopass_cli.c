#include "nss_octopass-passwd.c"
#include "nss_octopass-group.c"
#include "nss_octopass-shadow.c"
#include "nss_octopass.c"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

void show_pwent(struct passwd *pwent)
{
  printf("%s:%s:%ld:%ld:%s:%s:%s\n", pwent->pw_name, pwent->pw_passwd, pwent->pw_uid, pwent->pw_gid, pwent->pw_gecos,
         pwent->pw_dir, pwent->pw_shell);
}

void show_grent(struct group *grent)
{
  printf("%s:%s:%ld", grent->gr_name, grent->gr_passwd, grent->gr_gid);
  const int count = sizeof(grent->gr_mem) / sizeof(char);
  int i;

  for (i = 0; i < count; i++) {
    printf(":%s", grent->gr_mem[i]);
  }

  if (count == 0) {
    printf(":\n");
  } else {
    printf("\n");
  }
}

void show_spent(struct spwd *spent)
{
  if (spent->sp_lstchg == -1 && spent->sp_min == -1 && spent->sp_max == -1 && spent->sp_warn == -1 &&
      spent->sp_inact == -1 && spent->sp_expire == -1) {
    printf("%s:%s:::::::\n", spent->sp_namp, spent->sp_pwdp);
  } else {
    printf("%s:%s:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n", spent->sp_namp, spent->sp_pwdp, spent->sp_lstchg, spent->sp_min,
           spent->sp_max, spent->sp_warn, spent->sp_inact, spent->sp_expire, spent->sp_flag);
  }
}

void call_getpwnam_r(const char *name)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getpwnam_r(name, &pwent, buf, buflen, &err);
  show_pwent(&pwent);
}

void call_getpwuid_r(uid_t uid)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getpwuid_r(uid, &pwent, buf, buflen, &err);
  show_pwent(&pwent);
}

void call_pwlist(void)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status = _nss_octopass_setpwent(0);

  while (status == NSS_STATUS_SUCCESS) {
    status = _nss_octopass_getpwent_r(&pwent, buf, buflen, &err);
    if (status == NSS_STATUS_SUCCESS) {
      show_pwent(&pwent);
    }
  }

  status = _nss_octopass_endpwent();
}

void call_getgrnam_r(const char *name)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getgrnam_r(name, &grent, buf, buflen, &err);
  show_grent(&grent);
}

void call_getgrgid_r(gid_t gid)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getgrgid_r(gid, &grent, buf, buflen, &err);
  show_grent(&grent);
}

void call_grlist(void)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status  = _nss_octopass_setgrent(0);
  int max = json_array_size(ent_json_root);

  while (status == NSS_STATUS_SUCCESS) {
    status = _nss_octopass_getgrent_r(&grent, buf, buflen, &err);
    if (status == NSS_STATUS_SUCCESS) {
      show_grent(&grent);
    }
  }

  status = _nss_octopass_endgrent();
}

void call_getspnam_r(const char *name)
{
  enum nss_status status;
  struct spwd spent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status = _nss_octopass_getspnam_r(name, &spent, buf, buflen, &err);
  show_spent(&spent);
}

void call_splist(void)
{
  enum nss_status status;
  struct spwd spent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status = _nss_octopass_setspent(0);

  while (status == NSS_STATUS_SUCCESS) {
    status = _nss_octopass_getspent_r(&spent, buf, buflen, &err);
    if (status == NSS_STATUS_SUCCESS) {
      show_spent(&spent);
    }
  }

  status = _nss_octopass_endspent();
}

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
