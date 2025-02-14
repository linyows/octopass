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

#include "nss_octopass-group.c"

void show_grent(struct group *grent)
{
  if (!grent || !grent->gr_name || !grent->gr_passwd) {
    fprintf(stderr, "Error: Invalid group entry\n");
    return;
  }

  printf("%s:%s:%d", grent->gr_name, grent->gr_passwd, grent->gr_gid);

  if (grent->gr_mem) {
    for (int i = 0; grent->gr_mem[i] != NULL; i++) {
      printf(":%s", grent->gr_mem[i]);
    }
  }

  printf("\n");
}

void call_getgrnam_r(const char *name)
{
  enum nss_status status;
  struct group grent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_getgrnam_r(name, &grent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_grent(&grent);
  }

  free(buf);
}

void call_getgrgid_r(gid_t gid)
{
  enum nss_status status;
  struct group grent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_getgrgid_r(gid, &grent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_grent(&grent);
  }

  free(buf);
}

void call_grlist(void)
{
  enum nss_status status;
  struct group grent;
  int err = 0;
  int buflen = 2048;
  char *buf = malloc(buflen);
  if (!buf) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return;
  }

  status = _nss_octopass_setgrent(0);
  if (status != NSS_STATUS_SUCCESS) {
    fprintf(stderr, "Error: Failed to initialize group enumeration\n");
    free(buf);
    return;
  }

  while ((status = _nss_octopass_getgrent_r(&grent, buf, buflen, &err)) == NSS_STATUS_SUCCESS) {
    show_grent(&grent);
  }

  status = _nss_octopass_endgrent();
  if (status != NSS_STATUS_SUCCESS) {
    fprintf(stderr, "Warning: Failed to end group enumeration\n");
  }

  free(buf);
}
