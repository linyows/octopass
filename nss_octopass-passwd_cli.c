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
