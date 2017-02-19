#define OCTOPASS_CONFIG_FILE "octopass.conf.example"
#include <criterion/criterion.h>
#include "octopass.c"

void setup(void)
{
  char *token    = getenv("OCTOPASS_TOKEN");
  char *endpoint = getenv("OCTOPASS_ENDPOINT");
  if (!token || !endpoint) {
    cr_skip_test("Missing environment variables, token or endpoint");
  }
}

Test(nss_octopass, export_file)
{
  char *f = "/tmp/nss_octopass-export_file_test_1.txt";
  char *d = "LINE1\nLINE2\nLINE3\n";
  nss_octopass_export_file(f, d);

  FILE *fp = fopen(f, "r");

  if (fp == NULL) {
    cr_assert_fail("File open failure");
  }

  char line[10];
  int i = 0;

  while (fgets(line, sizeof(line), fp) != NULL) {
    switch (i) {
    case 0:
      cr_assert_str_eq(line, "LINE1\n");
      break;
    case 1:
      cr_assert_str_eq(line, "LINE2\n");
      break;
    case 2:
      cr_assert_str_eq(line, "LINE3\n");
      break;
    }
    i += 1;
  }

  fclose(fp);
}

Test(nss_octopass, import_file)
{
  char *f1 = "/tmp/nss_octopass-import_file_test_1.txt";
  char *d1 = "LINE1\nLINE2\nLINE3\n";
  nss_octopass_export_file(f1, d1);
  const char *data1 = nss_octopass_import_file(f1);
  cr_assert_str_eq(data1, d1);

  char *f2 = "/tmp/nss_octopass-import_file_test_2.txt";
  char *d2 = "LINEa\nLINEb\nLINEc\n";
  nss_octopass_export_file(f2, d2);
  const char *data2 = nss_octopass_import_file(f2);
  cr_assert_str_eq(data2, d2);
}

Test(nss_octopass, truncate)
{
  char *s          = "abcdefghijklmnopqrstuvwxyz0123456789!@#$";
  const char *res1 = nss_octopass_truncate(s, 6);
  cr_assert_str_eq(res1, "abcdef");

  const char *res2 = nss_octopass_truncate(s, 2);
  cr_assert_str_eq(res2, "ab");

  const char *res3 = nss_octopass_truncate(s, 300);
  cr_assert_str_eq(res3, s);
}

Test(nss_octopass, masking)
{
  char *s1          = "abcdefghijklmnopqrstuvwxyz0123456789!@#$";
  const char *mask1 = nss_octopass_masking(s1);
  cr_assert_str_eq(mask1, "abcde ************ REDACTED ************");

  char *s2          = "----------klmnopqrstuvwxyz0123456789!@#$";
  const char *mask2 = nss_octopass_masking(s2);
  cr_assert_str_eq(mask2, "----- ************ REDACTED ************");
}

Test(nss_octopass, url_normalization)
{
  char *base_url1  = "https://foo.com/api/v3";
  const char *url1 = octopass_url_normalization(base_url1);
  cr_assert_str_eq(url1, "https://foo.com/api/v3/");

  char *base_url2  = "https://foo.com/api/v3/";
  const char *url2 = octopass_url_normalization(base_url2);
  cr_assert_str_eq(url2, "https://foo.com/api/v3/");

  char *base_url3  = "https://api.github.com";
  const char *url3 = octopass_url_normalization(base_url3);
  cr_assert_str_eq(url3, "https://api.github.com/");

  char *base_url4  = "https://api.github.com/";
  const char *url4 = octopass_url_normalization(base_url4);
  cr_assert_str_eq(url4, "https://api.github.com/");
}

Test(nss_octopass, override_config_by_env)
{
  clearenv();

  struct config con;
  nss_octopass_override_config_by_env(&con);
  cr_assert_str_empty(con.token);
  cr_assert_str_empty(con.endpoint);
  cr_assert_str_empty(con.organization);
  cr_assert_str_empty(con.team);

  putenv("OCTOPASS_TOKEN=secret-token");
  putenv("OCTOPASS_ENDPOINT=https://api.github.com/");
  putenv("OCTOPASS_ORGANIZATION=octopass");
  putenv("OCTOPASS_TEAM=operation");
  nss_octopass_override_config_by_env(&con);
  cr_assert_str_eq(con.token, "secret-token");
  cr_assert_str_eq(con.endpoint, "https://api.github.com/");
  cr_assert_str_eq(con.organization, "octopass");
  cr_assert_str_eq(con.team, "operation");

  clearenv();
}

Test(nss_octopass, config_loading)
{
  clearenv();

  struct config con;
  char *f = "octopass.conf.example";
  nss_octopass_config_loading(&con, f);

  cr_assert_str_eq(con.endpoint, "https://your.github.com/api/v3/");
  cr_assert_str_eq(con.token, "iad87dih122ce66a1e20a751664c8a9dkoak87g7");
  cr_assert_str_eq(con.organization, "yourorganization");
  cr_assert_str_eq(con.team, "yourteam");
  cr_assert_str_eq(con.group_name, "yourteam");
  cr_assert_str_eq(con.home, "/home/%s");
  cr_assert_str_eq(con.shell, "/bin/bash");
  cr_assert_eq(con.uid_starts, 2000);
  cr_assert_eq(con.gid, 2000);
  cr_assert_eq(con.cache, 300);
  cr_assert(con.syslog == false);
}

Test(nss_octopass, remove_quotes)
{
  char s[] = "\"foo\"";
  nss_octopass_remove_quotes(&s[0]);

  cr_assert_str_eq(s, "foo");

  char s_contains[] = "\"I'm a \"foo\"\"";
  nss_octopass_remove_quotes(&s_contains[0]);

  cr_assert_str_eq(s_contains, "I'm a \"foo\"");
}

Test(nss_octopass, team_id, .init = setup)
{
  struct config con;
  char *f = "octopass.conf.example";
  nss_octopass_config_loading(&con, f);
  int id = nss_octopass_team_id(&con);

  cr_assert_eq(id, 2244789);
}
