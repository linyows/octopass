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
#include "octopass.c"

void setup(void)
{
  char *token    = getenv("OCTOPASS_TOKEN");
  char *endpoint = getenv("OCTOPASS_ENDPOINT");
  if (!token || !endpoint) {
    cr_skip_test("Missing environment variables, token or endpoint");
  }
}

Test(octopass, remove_quotes)
{
  char s[] = "\"foo\"";
  octopass_remove_quotes(&s[0]);

  cr_assert_str_eq(s, "foo");

  char s_contains[] = "\"I'm a \"foo\"\"";
  octopass_remove_quotes(&s_contains[0]);

  cr_assert_str_eq(s_contains, "I'm a \"foo\"");
}

Test(octopass, truncate)
{
  char *s          = "abcdefghijklmnopqrstuvwxyz0123456789!@#$";
  const char *res1 = octopass_truncate(s, 6);
  cr_assert_str_eq(res1, "abcdef");

  const char *res2 = octopass_truncate(s, 2);
  cr_assert_str_eq(res2, "ab");

  const char *res3 = octopass_truncate(s, 300);
  cr_assert_str_eq(res3, s);
}

Test(octopass, masking)
{
  char *s1          = "abcdefghijklmnopqrstuvwxyz0123456789!@#$";
  const char *mask1 = octopass_masking(s1);
  cr_assert_str_eq(mask1, "abcde ************ REDACTED ************");

  char *s2          = "----------klmnopqrstuvwxyz0123456789!@#$";
  const char *mask2 = octopass_masking(s2);
  cr_assert_str_eq(mask2, "----- ************ REDACTED ************");
}

Test(octopass, url_normalization)
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

Test(octopass, match)
{
  char *str     = "[ \"abc\", \"de\", \"F012\" ]";
  char *pattern = "\"([A-z0-9_-]+)\"";
  char **matched;
  matched = malloc(MAXBUF);
  int cnt = octopass_match(str, pattern, matched);

  cr_assert_eq(cnt, 3);
  cr_assert_str_eq(matched[0], (char *)"abc");
  cr_assert_str_eq(matched[1], (char *)"de");
  cr_assert_str_eq(matched[2], (char *)"F012");
  free(matched);
}

Test(octopass, override_config_by_env)
{
  clearenv();

  struct config con = { 0 };
  octopass_override_config_by_env(&con);
  cr_assert_str_empty(con.token);
  cr_assert_str_empty(con.endpoint);
  cr_assert_str_empty(con.organization);
  cr_assert_str_empty(con.team);
  cr_assert_str_empty(con.repository);

  putenv("OCTOPASS_TOKEN=secret-token");
  putenv("OCTOPASS_ENDPOINT=https://api.github.com/");
  putenv("OCTOPASS_ORGANIZATION=octopass");
  putenv("OCTOPASS_TEAM=operation");
  putenv("OCTOPASS_OWNER=linyows");
  putenv("OCTOPASS_REPOSITORY=foo");
  putenv("OCTOPASS_PERMISSION=admin");
  octopass_override_config_by_env(&con);
  cr_assert_str_eq(con.token, "secret-token");
  cr_assert_str_eq(con.endpoint, "https://api.github.com/");
  cr_assert_str_eq(con.organization, "octopass");
  cr_assert_str_eq(con.team, "operation");
  cr_assert_str_eq(con.owner, "linyows");
  cr_assert_str_eq(con.repository, "foo");
  cr_assert_str_eq(con.permission, "admin");

  clearenv();
}

Test(octopass, config_loading)
{
  clearenv();

  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);

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
  cr_assert_eq(con.shared_users_count, 2);
  cr_assert_str_eq(con.shared_users[0], (char *)"admin");
  cr_assert_str_eq(con.shared_users[1], (char *)"deploy");
}

Test(octopass, config_loading__when_use_repository)
{
  clearenv();

  struct config con;
  char *f = "test/octopass_repo.conf";
  octopass_config_loading(&con, f);

  cr_assert_str_eq(con.endpoint, "https://your.github.com/api/v3/");
  cr_assert_str_eq(con.token, "iad87dih122ce66a1e20a751664c8a9dkoak87g7");
  cr_assert_str_eq(con.owner, "yourorganization");
  cr_assert_str_eq(con.repository, "yourrepo");
  cr_assert_str_eq(con.permission, "write");
  cr_assert_str_eq(con.group_name, "yourrepo");
  cr_assert_str_eq(con.home, "/home/%s");
  cr_assert_str_eq(con.shell, "/bin/bash");
  cr_assert_eq(con.uid_starts, 2000);
  cr_assert_eq(con.gid, 2000);
  cr_assert_eq(con.cache, 300);
  cr_assert(con.syslog == false);
  cr_assert_eq(con.shared_users_count, 0);
}

Test(octopass, export_file)
{
  struct config con;
  char *cf = "test/octopass.conf";
  octopass_config_loading(&con, cf);

  char *dir = "/tmp";
  char *f = "/tmp/octopass-export_file_test_1.txt";
  char *d = "LINE1\nLINE2\nLINE3\n";
  octopass_export_file(&con, dir, f, d);

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

Test(octopass, import_file)
{
  struct config con;
  char *cf = "test/octopass.conf";
  octopass_config_loading(&con, cf);

  char *dir = "/tmp";
  char *f1 = "/tmp/octopass-import_file_test_1.txt";
  char *d1 = "LINE1\nLINE2\nLINE3\n";
  octopass_export_file(&con, dir, f1, d1);
  const char *data1 = octopass_import_file(&con, f1);
  cr_assert_str_eq(data1, d1);

  char *f2 = "/tmp/octopass-import_file_test_2.txt";
  char *d2 = "LINEa\nLINEb\nLINEc\n";
  octopass_export_file(&con, dir, f2, d2);
  const char *data2 = octopass_import_file(&con, f2);
  cr_assert_str_eq(data2, d2);
}

Test(octopass, github_request_without_cache, .init = setup)
{
  struct config con;
  struct response res;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *url = "https://api.github.com/";
  char *token;
  octopass_github_request_without_cache(&con, url, &res, token);

  cr_assert_eq((long *)200, res.httpstatus);
  /*
  cr_assert_str_eq(
      "{\"current_user_url\":\"https://api.github.com/user\",\"current_user_authorizations_html_url\":\"https://"
      "github.com/settings/connections/applications{/client_id}\",\"authorizations_url\":\"https://api.github.com/"
      "authorizations\",\"code_search_url\":\"https://api.github.com/search/"
      "code?q={query}{&page,per_page,sort,order}\",\"commit_search_url\":\"https://api.github.com/search/"
      "commits?q={query}{&page,per_page,sort,order}\",\"emails_url\":\"https://api.github.com/user/"
      "emails\",\"emojis_url\":\"https://api.github.com/emojis\",\"events_url\":\"https://api.github.com/"
      "events\",\"feeds_url\":\"https://api.github.com/feeds\",\"followers_url\":\"https://api.github.com/user/"
      "followers\",\"following_url\":\"https://api.github.com/user/following{/target}\",\"gists_url\":\"https://"
      "api.github.com/gists{/gist_id}\",\"hub_url\":\"https://api.github.com/hub\",\"issue_search_url\":\"https://"
      "api.github.com/search/issues?q={query}{&page,per_page,sort,order}\",\"issues_url\":\"https://api.github.com/"
      "issues\",\"keys_url\":\"https://api.github.com/user/keys\",\"notifications_url\":\"https://api.github.com/"
      "notifications\",\"organization_repositories_url\":\"https://api.github.com/orgs/{org}/"
      "repos{?type,page,per_page,sort}\",\"organization_url\":\"https://api.github.com/orgs/"
      "{org}\",\"public_gists_url\":\"https://api.github.com/gists/public\",\"rate_limit_url\":\"https://"
      "api.github.com/rate_limit\",\"repository_url\":\"https://api.github.com/repos/{owner}/"
      "{repo}\",\"repository_search_url\":\"https://api.github.com/search/"
      "repositories?q={query}{&page,per_page,sort,order}\",\"current_user_repositories_url\":\"https://api.github.com/"
      "user/repos{?type,page,per_page,sort}\",\"starred_url\":\"https://api.github.com/user/starred{/owner}{/"
      "repo}\",\"starred_gists_url\":\"https://api.github.com/gists/starred\",\"team_url\":\"https://api.github.com/"
      "teams\",\"user_url\":\"https://api.github.com/users/{user}\",\"user_organizations_url\":\"https://"
      "api.github.com/user/orgs\",\"user_repositories_url\":\"https://api.github.com/users/{user}/"
      "repos{?type,page,per_page,sort}\",\"user_search_url\":\"https://api.github.com/search/"
      "users?q={query}{&page,per_page,sort,order}\"}",
      res.data);
  */
}

Test(octopass, github_request_without_cache__when_use_dummy_token, .init = setup)
{
  struct config con;
  struct response res;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *url   = "https://api.github.com/";
  char *token = "dummydummydummydummydummydummydummydummy";
  octopass_github_request_without_cache(&con, url, &res, token);

  cr_assert_eq((long *)401, res.httpstatus);
  cr_assert_str_eq("{\"message\":\"Bad credentials\",\"documentation_url\":\"https://docs.github.com/rest\"}",
                   res.data);
}

Test(octopass, team_id, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  int id = octopass_team_id(&con);

  cr_assert_eq(id, 2244789);
}

Test(octopass, permission_level, .init = setup)
{
  char *adm = "admin";
  char *admin = octopass_permission_level(adm);
  cr_assert_str_eq(admin, "admin");

  char *write = "write";
  char *push = octopass_permission_level(write);
  cr_assert_str_eq(push, "push");

  char *read = "read";
  char *pull = octopass_permission_level(read);
  cr_assert_str_eq(pull, "pull");
}

Test(octopass, is_authorized_collaborator, .init = setup)
{
  putenv("OCTOPASS_OWNER=linyows");
  putenv("OCTOPASS_REPOSITORY=octopass");

  struct config con;
  struct response res;
  char *f = "test/octopass_repo.conf";
  octopass_config_loading(&con, f);
  int status1 = octopass_repository_collaborators(&con, &res);
  cr_assert_eq(status1, 0);

  size_t i;
  json_error_t error;
  json_t *collaborators;
  json_t *collaborator;
  collaborators = json_loads(res.data, 0, &error);
  cr_assert_eq(json_array_size(collaborators), 1);

  json_array_foreach(collaborators, i, collaborator) {
    int status2 = octopass_is_authorized_collaborator(&con, collaborator);
    cr_assert_eq(status2, 1);
  }

  clearenv();
}

Test(octopass, rebuild_data_with_authorized, .init = setup)
{
  putenv("OCTOPASS_OWNER=linyows");
  putenv("OCTOPASS_REPOSITORY=octopass");

  struct config con;
  struct response res;
  char *f = "test/octopass_repo.conf";
  octopass_config_loading(&con, f);

  char *stub = "test/collaborators.json";
  res.httpstatus = (long *)200;
  res.data = (char *)octopass_import_file(&con, stub);
  res.size = strlen(res.data);
  octopass_rebuild_data_with_authorized(&con, &res);

  size_t i;
  json_error_t error;
  json_t *collaborators;
  json_t *collaborator;
  collaborators = json_loads(res.data, 0, &error);

  cr_assert_eq(json_array_size(collaborators), 1);

  json_t *me = json_array_get(collaborators, 0);
  const char *login = json_string_value(json_object_get(me, "login"));
  cr_assert_str_eq(login, "linyows");

  clearenv();
}

Test(octopass, rebuild_data_with_authorized__when_permission_is_read, .init = setup)
{
  putenv("OCTOPASS_OWNER=linyows");
  putenv("OCTOPASS_REPOSITORY=octopass");
  putenv("OCTOPASS_PERMISSION=read");

  struct config con;
  struct response res;
  char *f = "test/octopass_repo.conf";
  octopass_config_loading(&con, f);

  char *stub = "test/collaborators.json";
  res.httpstatus = (long *)200;
  res.data = (char *)octopass_import_file(&con, stub);
  res.size = strlen(res.data);
  octopass_rebuild_data_with_authorized(&con, &res);

  size_t i;
  json_error_t error;
  json_t *collaborators;
  json_t *collaborator;
  collaborators = json_loads(res.data, 0, &error);

  cr_assert_eq(json_array_size(collaborators), 2);

  json_t *me = json_array_get(collaborators, 0);
  const char *login1 = json_string_value(json_object_get(me, "login"));
  cr_assert_str_eq(login1, "linyows");

  json_t *other = json_array_get(collaborators, 1);
  const char *login2 = json_string_value(json_object_get(other, "login"));
  cr_assert_str_eq(login2, "nolinyows");

  clearenv();
}

Test(octopass, repository_collaborators, .init = setup)
{
  putenv("OCTOPASS_OWNER=linyows");
  putenv("OCTOPASS_REPOSITORY=octopass");

  struct config con;
  struct response res;
  char *f = "test/octopass_repo.conf";
  octopass_config_loading(&con, f);
  int status = octopass_repository_collaborators(&con, &res);
  cr_assert_eq(status, 0);

  json_error_t error;
  json_t *collaborators = json_loads(res.data, 0, &error);
  cr_assert_eq(json_array_size(collaborators), 1);
  json_t *me = json_array_get(collaborators, 0);
  const char *login = json_string_value(json_object_get(me, "login"));
  cr_assert_str_eq(login, "linyows");

  clearenv();
}

Test(octopass, authentication_with_token, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *user  = "linyows";
  char *token = getenv("OCTOPASS_TOKEN");
  int status  = octopass_autentication_with_token(&con, user, token);
  cr_assert_eq(status, 0);
}

Test(octopass, authentication_with_token__when_wrong_user, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *user  = "dummy";
  char *token = getenv("OCTOPASS_TOKEN");
  int status  = octopass_autentication_with_token(&con, user, token);
  cr_assert_eq(status, 1);
}

Test(octopass, authentication_with_token__when_wrong_token, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *user  = "linyows";
  char *token = "dummydummydummydummydummydummydummydummy";
  int status  = octopass_autentication_with_token(&con, user, token);
  cr_assert_eq(status, 1);
}

Test(octopass, github_user_keys, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  char *user       = "linyows";
  const char *keys = octopass_github_user_keys(&con, user);
  cr_assert_str_eq(keys,
                   "ssh-rsa "
                   "AAAAB3NzaC1yc2EAAAADAQABAAABAQDpfOPDOHf5ZpFLR2dMhK+B3vSMtAlh/"
                   "HPOQXsolZYmPQW/xGb0U0+rgXVvBEw193q5c236ENdSrk4R2NE/4ipA/awyCYCJG78Llj2Sm"
                   "qPWbuCtv1K06mXwuh6VM3DP1wPGJmWnzf44Eee4NtTvOzMrORdvGtzQAM044h11N24w07vYwlBvW3P+PdxllbBD"
                   "Jv0ns2A1v40Oerh/xLqAN6UpUADv5prPAnpGnVmuhiNHElX96FmY4y1RxW"
                   "FNyxnb7/wRwp0NnjfTAmJtB9SWJK9UABLfre2HHlX0gBbhj1+LSW+U5jXD8F9"
                   "BZF4XRtVY3Ep0PnUrdDqjttrYE0mBfsMh\nssh-rsa "
                   "AAAAB3NzaC1yc2EAAAADAQABAAABAQCbBkU87QyUEmecsQjCcMTdS6iARCUXzMo2awb4c+irGPUv"
                   "kXxQUljmLFRXCIw+cEKajiS7VY5NLCZ6WVCbd4yIK"
                   "+3jdNzrf74isiG8GdU+m64gNGaXtKGFaQEXBp9uWqqZgSw+bVMX2ArOtoh3lP96WJQOoXsOuX0izNS5qf1Z9E01J6IpE3xf"
                   "udpaL4/vY1RnljM+KUoiIPFqS1Q7kJ+8LpHvV1T9SRiocpLThXOzifzwwoo9I6emwHr+kGwODE"
                   "RYWYvkMEwFyOh8fKAcTdt8huUz8n6k59V9y5hZWDuxP/zhnArUMwWHiiS1C5im8baX8jxSW6RoHuetBxSUn5vR\n");
}

Test(octopass, github_team_members_keys, .init = setup)
{
  struct config con;
  char *f = "test/octopass.conf";
  octopass_config_loading(&con, f);
  const char *keys = octopass_github_team_members_keys(&con);
  cr_assert_str_eq(keys,
                   "ssh-rsa "
                   "AAAAB3NzaC1yc2EAAAADAQABAAABAQDpfOPDOHf5ZpFLR2dMhK+B3vSMtAlh/"
                   "HPOQXsolZYmPQW/xGb0U0+rgXVvBEw193q5c236ENdSrk4R2NE/4ipA/awyCYCJG78Llj2Sm"
                   "qPWbuCtv1K06mXwuh6VM3DP1wPGJmWnzf44Eee4NtTvOzMrORdvGtzQAM044h11N24w07vYwlBvW3P+PdxllbBD"
                   "Jv0ns2A1v40Oerh/xLqAN6UpUADv5prPAnpGnVmuhiNHElX96FmY4y1RxW"
                   "FNyxnb7/wRwp0NnjfTAmJtB9SWJK9UABLfre2HHlX0gBbhj1+LSW+U5jXD8F9"
                   "BZF4XRtVY3Ep0PnUrdDqjttrYE0mBfsMh\nssh-rsa "
                   "AAAAB3NzaC1yc2EAAAADAQABAAABAQCbBkU87QyUEmecsQjCcMTdS6iARCUXzMo2awb4c+irGPUv"
                   "kXxQUljmLFRXCIw+cEKajiS7VY5NLCZ6WVCbd4yIK"
                   "+3jdNzrf74isiG8GdU+m64gNGaXtKGFaQEXBp9uWqqZgSw+bVMX2ArOtoh3lP96WJQOoXsOuX0izNS5qf1Z9E01J6IpE3xf"
                   "udpaL4/vY1RnljM+KUoiIPFqS1Q7kJ+8LpHvV1T9SRiocpLThXOzifzwwoo9I6emwHr+kGwODE"
                   "RYWYvkMEwFyOh8fKAcTdt8huUz8n6k59V9y5hZWDuxP/zhnArUMwWHiiS1C5im8baX8jxSW6RoHuetBxSUn5vR\n"
                   "ssh-rsa "
                   "AAAAB3NzaC1yc2EAAAADAQABAAACAQCnzpR1gnRCfNTpvMGWiXLqjFxqgMN23hy2Q55ac9KJJXMTf1q1ZOKrt0EC6Bt/"
                   "r7M7bo3EzmaIbOrTDxPtVpKgqHNpS31n6beVy/"
                   "pukhcdWq0C6KI1miXpySZoWf2j05foDKMhvO2t15NddU9qmQn7fWwvCEwUKv13lAETSQ6ZKR1A7hFfNAFBxLACCyhvNpvVcVD1p"
                   "bCymygENBOjtpIFSHFSdCBpm3FpVrH4sFbQZZyTicJW0sLMnHjdqlrM9F/"
                   "GF+eGRdejSPpjZh6rNX54QMIR+oEyBqatFD6LH0lgWvw+VDwCHFaXQVbdLlRCQDxAH0ocMPHH4ZurYf69oDah73xfh3fCb9v+B+"
                   "gp+4zN7MqcYZy15zw/"
                   "PIKHySWitST1hi+uLwa3FXGIgHdMtFFrkR9hrpSqRwPduUanzU01jl1gmq237SKy0fuzmvUKdZPRkrLz4cNfdD++"
                   "S6v4nrb8JHryp0Hp6NJ02XDtylKQJJX9ZL7lsxE/"
                   "PCdMiG0CngdSe1MY2TV0tCt3TNvDfmk7zB4pe3GuOdGazIdlv2aukEAnSNmoVUflym08J4Oz676PI0onFUExEfok21AQNtt6WsU"
                   "bi7Jd84Xx09HLjMFDPYyZQZWn9+u5ZIaAVPwg26T3w8FU/OJmKK+Autdm3VBc2UHTmDlL2qdrtLjcQZAMqw==\n");
}
