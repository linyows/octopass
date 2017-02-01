#include "nss_octopass-passwd.c"

void show_pwent(struct passwd *pwent)
{
  printf("%s:%s:%d:%d:%s:%s:%s\n", pwent->pw_name, pwent->pw_passwd, pwent->pw_uid, pwent->pw_gid, pwent->pw_gecos,
         pwent->pw_dir, pwent->pw_shell);
}

void call_getpwnam_r(const char *name)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getpwnam_r(name, &pwent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_pwent(&pwent);
  }
}

void call_getpwuid_r(uid_t uid)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getpwuid_r(uid, &pwent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_pwent(&pwent);
  }
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
