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

#include "nss_octopass-shadow.h"

void show_spent(struct spwd *spent)
{
  if (!spent || !spent->sp_namp || !spent->sp_pwdp) {
    fprintf(stderr, "Error: Invalid shadow entry\n");
    return;
  }

  if (spent->sp_lstchg == -1 && spent->sp_min == -1 && spent->sp_max == -1 && spent->sp_warn == -1 &&
    spent->sp_inact == -1 && spent->sp_expire == -1) {
    printf("%s:%s:::::::\n",
           spent->sp_namp ? spent->sp_namp : "N/A",
           spent->sp_pwdp ? spent->sp_pwdp : "N/A");
  } else {
    printf("%s:%s:%ld:%ld:%ld:%ld:%ld:%ld:%ld\n",
           spent->sp_namp ? spent->sp_namp : "N/A",
           spent->sp_pwdp ? spent->sp_pwdp : "N/A",
           spent->sp_lstchg,
           spent->sp_min,
           spent->sp_max,
           spent->sp_warn,
           spent->sp_inact,
           spent->sp_expire,
           spent->sp_flag);
  }
}

void call_getspnam_r(const char *name)
{
  enum nss_status status;
  struct spwd spent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_getspnam_r(name, &spent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_spent(&spent);
  } else {
    fprintf(stderr, "Error: Failed to retrieve shadow entry for %s\n", name);
  }

  free(buf);
}

void call_splist(void)
{
  enum nss_status status;
  struct spwd spent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_setspent(0);
  if (status != NSS_STATUS_SUCCESS) {
    fprintf(stderr, "Error: Failed to initialize shadow enumeration\n");
    free(buf);
    return;
  }

  while ((status = _nss_octopass_getspent_r(&spent, buf, buflen, &err)) == NSS_STATUS_SUCCESS) {
    show_spent(&spent);
  }

  status = _nss_octopass_endspent();
  if (status != NSS_STATUS_SUCCESS) {
    fprintf(stderr, "Warning: Failed to end shadow enumeration\n");
  }

  free(buf);
}
