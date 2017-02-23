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

#define OCTOPASS_CONFIG_FILE "test/octopass.conf"
#include <criterion/criterion.h>
#include "nss_octopass-passwd.c"

extern void setup(void);

Test(nss_octopass, getpwnam_r, .init = setup)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "linyows";
  status           = _nss_octopass_getpwnam_r(name, &pwent, buf, buflen, &err);

  cr_assert_eq(err, 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
  cr_assert_str_eq(pwent.pw_name, "linyows");
  cr_assert_str_eq(pwent.pw_passwd, "x");
  cr_assert_eq(pwent.pw_uid, 74049);
  cr_assert_eq(pwent.pw_gid, 2000);
  cr_assert_str_eq(pwent.pw_gecos, "managed by octopass");
  cr_assert_str_eq(pwent.pw_dir, "/home/linyows");
  cr_assert_str_eq(pwent.pw_shell, "/bin/bash");
}

Test(nss_octopass, getpwnam_r__when_team_member_not_found, .init = setup)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  const char *name = "linyowsno";
  status           = _nss_octopass_getpwnam_r(name, &pwent, buf, buflen, &err);

  cr_assert_eq(err, ENOENT);
  cr_assert_eq(status, NSS_STATUS_NOTFOUND);
}

Test(nss_octopass, pwent_list, .init = setup)
{
  enum nss_status status;
  struct passwd pwent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setpwent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    err    = 0;
    status = _nss_octopass_getpwent_r(&pwent, buf, buflen, &err);
    if (status != NSS_STATUS_SUCCESS) {
      continue;
    }

    cr_assert_eq(ent_json_idx, entry_number);
    cr_assert_eq(json_is_array(ent_json_root), 1);
    cr_assert_eq(status, NSS_STATUS_SUCCESS);

    if (strcmp(pwent.pw_name, "linyows") != 0) {
      continue;
    }

    cr_assert_str_eq(pwent.pw_name, "linyows");
    cr_assert_str_eq(pwent.pw_passwd, "x");
    cr_assert_eq(pwent.pw_uid, 74049);
    cr_assert_eq(pwent.pw_gid, 2000);
    cr_assert_str_eq(pwent.pw_gecos, "managed by octopass");
    cr_assert_str_eq(pwent.pw_dir, "/home/linyows");
    cr_assert_str_eq(pwent.pw_shell, "/bin/bash");
  }

  status = _nss_octopass_endpwent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
}

Test(nss_octopass, pwent_list__when_team_not_exist, .init = setup)
{
  putenv("OCTOPASS_TEAM=team_not_exists");

  enum nss_status status;
  struct passwd pwent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setpwent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_UNAVAIL);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    err    = 0;
    status = _nss_octopass_getpwent_r(&pwent, buf, buflen, &err);
    cr_assert_eq(status, NSS_STATUS_UNAVAIL);
    cr_assert_eq(ent_json_idx, 0);
    cr_assert_eq(ent_json_root, NULL);
  }

  status = _nss_octopass_endpwent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  clearenv();
}

Test(nss_octopass, pwent_list__when_wrong_token, .init = setup)
{
  putenv("OCTOPASS_TOKEN=wrong_token");

  enum nss_status status;
  struct passwd pwent;
  int err                    = 0;
  unsigned long entry_number = 0;
  int buflen                 = 2048;
  char buf[buflen];

  status = _nss_octopass_setpwent(0);
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(json_object_size(ent_json_root), 0);
  cr_assert_eq(status, NSS_STATUS_UNAVAIL);

  while (status == NSS_STATUS_SUCCESS) {
    entry_number += 1;
    err    = 0;
    status = _nss_octopass_getpwent_r(&pwent, buf, buflen, &err);
    cr_assert_eq(status, NSS_STATUS_UNAVAIL);
    cr_assert_eq(ent_json_idx, 0);
    cr_assert_eq(ent_json_root, NULL);
  }

  status = _nss_octopass_endpwent();
  cr_assert_eq(ent_json_idx, 0);
  cr_assert_eq(ent_json_root, NULL);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);

  clearenv();
}

Test(nss_octopass, getpwuid_r, .init = setup)
{
  enum nss_status status;
  struct passwd pwent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  uid_t uid = 74049;
  status    = _nss_octopass_getpwuid_r(uid, &pwent, buf, buflen, &err);

  cr_assert_eq(err, 0);
  cr_assert_eq(status, NSS_STATUS_SUCCESS);
  cr_assert_str_eq(pwent.pw_name, "linyows");
  cr_assert_str_eq(pwent.pw_passwd, "x");
  cr_assert_eq(pwent.pw_uid, 74049);
  cr_assert_eq(pwent.pw_gid, 2000);
  cr_assert_str_eq(pwent.pw_gecos, "managed by octopass");
  cr_assert_str_eq(pwent.pw_dir, "/home/linyows");
  cr_assert_str_eq(pwent.pw_shell, "/bin/bash");
}
