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
  if (!pwent || !pwent->pw_name || !pwent->pw_passwd || !pwent->pw_gecos || !pwent->pw_dir || !pwent->pw_shell) {
    fprintf(stderr, "Error: Invalid passwd entry\n");
    return;
  }

  printf("%s:%s:%d:%d:%s:%s:%s\n",
         pwent->pw_name ? pwent->pw_name : "N/A",
         pwent->pw_passwd ? pwent->pw_passwd : "N/A",
         pwent->pw_uid,
         pwent->pw_gid,
         pwent->pw_gecos ? pwent->pw_gecos : "N/A",
         pwent->pw_dir ? pwent->pw_dir : "N/A",
         pwent->pw_shell ? pwent->pw_shell : "N/A");
}

void call_getpwnam_r(const char *name)
{
  enum nss_status status;
  struct passwd pwent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_getpwnam_r(name, &pwent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_pwent(&pwent);
  } else {
    fprintf(stderr, "Error: Failed to retrieve passwd entry for %s\n", name);
  }

  free(buf);
}

void call_getpwuid_r(uid_t uid)
{
  enum nss_status status;
  struct passwd pwent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_getpwuid_r(uid, &pwent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_pwent(&pwent);
  }

  free(buf);
}

void call_pwlist(void)
{
  enum nss_status status;
  struct passwd pwent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_setpwent(0);
  if (status != NSS_STATUS_SUCCESS) {
    fprintf(stderr, "Error: Failed to initialize passwd enumeration\n");
    free(buf);
    return;
  }

  while ((status = _nss_octopass_getpwent_r(&pwent, buf, buflen, &err)) == NSS_STATUS_SUCCESS) {
    show_pwent(&pwent);
  }

  _nss_octopass_endpwent();
  free(buf);
}
