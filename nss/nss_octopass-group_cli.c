#include "nss_octopass-group.c"

void show_grent(struct group *grent)
{
  printf("%s:%s:%d", grent->gr_name, grent->gr_passwd, grent->gr_gid);
  const int count = json_array_size(ent_json_root);
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

void call_getgrnam_r(const char *name)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getgrnam_r(name, &grent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_grent(&grent);
  }
}

void call_getgrgid_r(gid_t gid)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];
  status = _nss_octopass_getgrgid_r(gid, &grent, buf, buflen, &err);
  if (status == NSS_STATUS_SUCCESS) {
    show_grent(&grent);
  }
}

void call_grlist(void)
{
  enum nss_status status;
  struct group grent;
  int err    = 0;
  int buflen = 2048;
  char buf[buflen];

  status = _nss_octopass_setgrent(0);

  while (status == NSS_STATUS_SUCCESS) {
    status = _nss_octopass_getgrent_r(&grent, buf, buflen, &err);
    if (status == NSS_STATUS_SUCCESS) {
      show_grent(&grent);
    }
  }

  status = _nss_octopass_endgrent();
}
