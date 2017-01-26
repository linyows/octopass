//#define NSS_OCTOPASS_CACHE 1
#include <criterion/criterion.h>
#include "nss_octopass.c"

void setup(void)
{
  char *token    = getenv("GITHUB_TOKEN");
  char *endpoint = getenv("GITHUB_ENDPOINT");
  if (!token || !endpoint) {
    cr_skip_test("Missing environment variables, token or endpoint");
  }
}

Test(nss_octopass, export_file)
{
  char *f = "/tmp/__test__.txt";
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
  char *f = "/tmp/__test__.txt";
  char *d = "LINE1\nLINE2\nLINE3\n";
  nss_octopass_export_file(f, d);
  const char *data = nss_octopass_import_file(f);
  cr_assert_str_eq(data, d);
}

Test(nss_octopass, masking)
{
  char *s          = "abcdefghijklmnopqrstuvwxyz0123456789!@#$";
  const char *mask = nss_octopass_masking(s);
  cr_assert_str_eq(mask, "abcde ************ REDACTED ************");
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

  putenv("GITHUB_TOKEN=secret-token");
  putenv("GITHUB_ENDPOINT=https://api.github.com");
  putenv("GITHUB_ORGANIZATION=octopass");
  putenv("GITHUB_TEAM=operation");
  nss_octopass_override_config_by_env(&con);
  cr_assert_str_eq(con.token, "secret-token");
  cr_assert_str_eq(con.endpoint, "https://api.github.com");
  cr_assert_str_eq(con.organization, "octopass");
  cr_assert_str_eq(con.team, "operation");

  clearenv();
}

Test(nss_octopass, config_loading)
{
  clearenv();

  struct config con;
  char *f = "example.octopass.conf";
  nss_octopass_config_loading(&con, f);

  cr_assert_str_eq(con.endpoint, "https://your.github.com/api/v3/");
  cr_assert_str_eq(con.token, "iad87dih122ce66a1e20a751664c8a9dkoak87g7");
  cr_assert_str_eq(con.organization, "yourorganization");
  cr_assert_str_eq(con.team, "yourteam");
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
  char *f = "example.octopass.conf";
  nss_octopass_config_loading(&con, f);
  int id = nss_octopass_team_id(&con);

  cr_assert_eq(id, 2244789);
}

Test(nss_octopass, id_by_name, .init = setup)
{
  struct config con;
  struct response res;
  char *name = "linyows";
  char *f    = "example.octopass.conf";
  nss_octopass_config_loading(&con, f);
  int id = nss_octopass_id_by_name(&con, &res, name);

  cr_assert_eq(id, 72049);
}
