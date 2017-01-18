#include <criterion/criterion.h>
#include "nss_octopass.c"

Test(nss_octopass, config_loading)
{
  struct config con;
  char *f = "example.octopass.conf";
  nss_octopass_config_loading(&con, f);

  cr_assert_str_eq(con.endpoint, "https://your.github.com/api/v3/");
  cr_assert_str_eq(con.token, "iad87dih122ce66a1e20a751664c8a9dkoak87g7");
  cr_assert_str_eq(con.organization, "yourorganization");
  cr_assert_str_eq(con.team, "yourteam");
  cr_assert(con.syslog == true);
}

Test(nss_octopass, remove_quotes)
{
  char s[] = "\"I'm a foo\"";
  nss_octopass_remove_quotes(&s);

  cr_assert_str_eq(s, "I'm a foo");
}
