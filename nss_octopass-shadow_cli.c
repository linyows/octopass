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

#include "nss_octopass-shadow.c"

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

void call_getspnam_r(const char *name)
{
  enum nss_status status;
  struct spwd spent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status = _nss_octopass_getspnam_r(name, &spent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_spent(&spent);
  }
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
