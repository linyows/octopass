#define NSS_OCTOPASS_CONFIG_FILE "example.octopass.conf"
#include <criterion/criterion.h>
#include "nss_octopass-group.c"

Test(nss_octopass, getgrnam_r)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "admin";
  status           = _nss_octopass_getgrnam_r(name, &grent, buf, buflen, &err);

  cr_assert_eq(err, 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
  cr_assert_str_eq(grent.gr_name, "admin");
  cr_assert_str_eq(grent.gr_passwd, "x");
  cr_assert_eq(grent.gr_gid, 2000);
  cr_assert_str_eq(grent.gr_mem[0], "linyows");
}

Test(nss_octopass, getgrnam_r__when_team_member_not_found)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "adminno";
  status           = _nss_octopass_getgrnam_r(name, &grent, buf, buflen, &err);

  cr_assert_eq(err, ENOENT);
  cr_assert_eq(status, NSS_STATUS_NOTFOUND);
}
