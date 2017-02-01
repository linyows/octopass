#define NSS_OCTOPASS_CONFIG_FILE "example.octopass.conf"
#include <criterion/criterion.h>
#include "nss_octopass-shadow.c"

extern void setup(void);

Test(nss_octopass, getspnam_r, .init = setup)
{
  enum nss_status status;
  struct spwd spent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "linyows";
  status           = _nss_octopass_getspnam_r(name, &spent, buf, buflen, &err);

  cr_assert_eq(err, 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
  cr_assert_str_eq(spent.sp_namp, "linyows");
  cr_assert_str_eq(spent.sp_pwdp, "!!");
  cr_assert_eq(spent.sp_lstchg, -1);
  cr_assert_eq(spent.sp_min, -1);
  cr_assert_eq(spent.sp_max, -1);
  cr_assert_eq(spent.sp_warn, -1);
  cr_assert_eq(spent.sp_inact, -1);
  cr_assert_eq(spent.sp_expire, -1);
  cr_assert_eq(spent.sp_flag, ~0ul);
}

Test(nss_octopass, getspnam_r__when_team_member_not_found, .init = setup)
{
  enum nss_status status;
  struct spwd spent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "linyowsno";
  status           = _nss_octopass_getspnam_r(name, &spent, buf, buflen, &err);

  cr_assert_eq(err, ENOENT);
  cr_assert_eq(status, NSS_STATUS_NOTFOUND);
}

Test(nss_octopass, spent_list, .init = setup)
{
  enum nss_status status;
  struct spwd spent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setspent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    status = _nss_octopass_getspent_r(&spent, buf, buflen, &err);
    if (status != NSS_STATUS_SUCCESS) {
      continue;
    }

    cr_assert_eq(ent_json_idx, entry_number);
    cr_assert_eq(json_is_array(ent_json_root), 1);
    cr_assert_eq(status, NSS_STATUS_SUCCESS);

    if (strcmp(spent.sp_namp, "linyows") != 0) {
      printf("Unknown user: %s(%lu)\n", spent.sp_namp, entry_number);
      continue;
    }

    cr_assert_str_eq(spent.sp_namp, "linyows");
    cr_assert_str_eq(spent.sp_pwdp, "!!");
    cr_assert_eq(spent.sp_lstchg, -1);
    cr_assert_eq(spent.sp_min, -1);
    cr_assert_eq(spent.sp_max, -1);
    cr_assert_eq(spent.sp_warn, -1);
    cr_assert_eq(spent.sp_inact, -1);
    cr_assert_eq(spent.sp_expire, -1);
    cr_assert_eq(spent.sp_flag, ~0ul);
  }

  status = _nss_octopass_endspent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
}

Test(nss_octopass, spent_list__when_team_not_exist, .init = setup)
{
  putenv("GITHUB_TEAM=team_not_exists");

  enum nss_status status;
  struct spwd spent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setspent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_UNAVAIL);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    status = _nss_octopass_getspent_r(&spent, buf, buflen, &err);
    cr_assert_eq(status, NSS_STATUS_UNAVAIL);
    cr_assert_eq(ent_json_idx, 0);
    cr_assert_eq(ent_json_root, NULL);
  }

  status = _nss_octopass_endspent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  clearenv();
}

Test(nss_octopass, spent_list__when_wrong_token, .init = setup)
{
  putenv("GITHUB_TOKEN=wrong_token");

  enum nss_status status;
  struct spwd spent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setspent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_UNAVAIL);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    status = _nss_octopass_getspent_r(&spent, buf, buflen, &err);
    cr_assert_eq(status, NSS_STATUS_UNAVAIL);
    cr_assert_eq(ent_json_idx, 0);
    cr_assert_eq(ent_json_root, NULL);
  }

  status = _nss_octopass_endspent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  clearenv();
}
