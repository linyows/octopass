#include "nss_octopass.h"

static size_t write_response_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize      = size * nmemb;
  struct response *res = (struct response *)userp;

  res->data = realloc(res->data, res->size + realsize + 1);
  if (res->data == NULL) {
    // out of memory!
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(res->data[res->size]), contents, realsize);
  res->size += realsize;
  res->data[res->size] = 0;

  return realsize;
}

void nss_octopass_remove_quotes(char *s)
{
  if (s == NULL) {
    return;
  }

  if (s[strlen(s) - 1] == '"') {
    s[strlen(s) - 1] = '\0';
  }

  int i = 0;
  while (s[i] != '\0' && s[i] == '"')
    i++;
  memcpy(s, &s[i], strlen(s));
}

const char *nss_octopass_truncate(const char *str, int len)
{
  char s[len + 1];
  strncpy(s, str, len);
  *(s + len) = '\0';
  char *res  = strdup(s);
  return res;
}

const char *nss_octopass_masking(const char *token)
{
  int len = 5;
  char s[strlen(token) + 1];
  sprintf(s, "%s ************ REDACTED ************", nss_octopass_truncate(token, len));
  char *mask = strdup(s);
  return mask;
}

int *nss_octopass_intdup(int const *src, size_t len)
{
  int *p = malloc(len * sizeof(int));
  memcpy(p, src, len * sizeof(int));
  return p;
}

void nss_octopass_override_config_by_env(struct config *con)
{
  char *token = getenv("GITHUB_TOKEN");
  if (token) {
    sprintf(con->token, "%s", token);
  }

  char *endpoint = getenv("GITHUB_ENDPOINT");
  if (endpoint) {
    sprintf(con->endpoint, "%s", endpoint);
  }

  char *org = getenv("GITHUB_ORGANIZATION");
  if (org) {
    sprintf(con->organization, "%s", org);
  }

  char *team = getenv("GITHUB_TEAM");
  if (team) {
    sprintf(con->team, "%s", team);
  }
}

void nss_octopass_config_loading(struct config *con, char *filename)
{
  memset(con->endpoint, '\0', sizeof(con->endpoint));
  memset(con->token, '\0', sizeof(con->token));
  memset(con->organization, '\0', sizeof(con->organization));
  memset(con->team, '\0', sizeof(con->team));
  memset(con->group_name, '\0', sizeof(con->group_name));
  memset(con->home, '\0', sizeof(con->home));
  memset(con->shell, '\0', sizeof(con->shell));
  con->uid_starts = (long)0;
  con->gid        = (long)0;

  FILE *file = fopen(filename, "r");

  if (file == NULL) {
    fprintf(stderr, "Config not found: %s\n", filename);
    exit(1);
  }

  char line[MAXBUF];

  while (fgets(line, sizeof(line), file) != NULL) {
    if (strlen(line) != sizeof(line) - 1) {
      line[strlen(line) - 1] = '\0';
    }

    char *lasts;
    char *key   = strtok_r(line, DELIM, &lasts);
    char *value = strtok_r(NULL, DELIM, &lasts);
    nss_octopass_remove_quotes(value);

    if (strcmp(key, "Endpoint") == 0) {
      memcpy(con->endpoint, value, strlen(value));
    } else if (strcmp(key, "Token") == 0) {
      memcpy(con->token, value, strlen(value));
    } else if (strcmp(key, "Organization") == 0) {
      memcpy(con->organization, value, strlen(value));
    } else if (strcmp(key, "Team") == 0) {
      memcpy(con->team, value, strlen(value));
    } else if (strcmp(key, "Home") == 0) {
      memcpy(con->home, value, strlen(value));
    } else if (strcmp(key, "Shell") == 0) {
      memcpy(con->shell, value, strlen(value));
    } else if (strcmp(key, "UidStarts") == 0) {
      con->uid_starts = atoi(value);
    } else if (strcmp(key, "Gid") == 0) {
      con->gid = atoi(value);
    } else if (strcmp(key, "GroupName") == 0) {
      memcpy(con->group_name, value, strlen(value));
    } else if (strcmp(key, "Syslog") == 0) {
      if (strcmp(value, "true") == 0) {
        con->syslog = true;
      } else {
        con->syslog = false;
      }
    }
  }

  nss_octopass_override_config_by_env(con);

  if (strlen(con->group_name) == 0) {
    memcpy(con->group_name, con->team, strlen(con->team));
  }

  if (!con->uid_starts) {
    con->uid_starts = (long)2000;
  }

  if (!con->gid) {
    con->gid = (long)2000;
  }

  if (strlen(con->home) == 0) {
    char *home = "/home/%s";
    memcpy(con->home, home, strlen(home));
  }

  if (strlen(con->shell) == 0) {
    char *shell = "/bin/bash";
    memcpy(con->shell, shell, strlen(shell));
  }

  fclose(file);

  if (con->syslog) {
    const char *pg_name = "nss-octopass";
    openlog(pg_name, LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "config {endpoint: %s, token: %s, organization: %s, team: %s, syslog: %d, "
                     "uid_starts: %ld, gid: %ld, group_name: %s, home: %s, shell: %s}",
           con->endpoint, nss_octopass_masking(con->token), con->organization, con->team, con->syslog, con->uid_starts,
           con->gid, con->group_name, con->home, con->shell);
  }
}

void nss_octopass_export_file(char *file, char *data)
{
  FILE *fp = fopen(file, "w");
  if (!fp) {
    fprintf(stderr, "File open failure: %s\n", file);
    exit(1);
    return;
  }
  fprintf(fp, data);
  fclose(fp);
}

const char *nss_octopass_import_file(char *file)
{
  FILE *fp = fopen(file, "r");
  if (!fp) {
    fprintf(stderr, "File open failure: %s\n", file);
    exit(1);
    return;
  }
  char line[MAXBUF];
  char *data;

  if ((data = malloc(MAXBUF * sizeof(char))) != NULL) {
    data[0] = '\0';
  } else {
    fprintf(stderr, "Malloc failed\n");
  }

  while (fgets(line, sizeof(line), fp) != NULL) {
    strcat(data, strdup(line));
  }
  fclose(fp);
  const char *res = strdup(data);
  free(data);

  return res;
}

void nss_octopass_github_request_without_cache(struct config *con, char *url, struct response *res)
{
  if (con->syslog) {
    syslog(LOG_INFO, "http get -- %s", url);
  }

  char auth[64];
  sprintf(auth, "Authorization: token %s", con->token);

  CURL *hnd;
  CURLcode result;
  struct curl_slist *headers = NULL;
  res->data                  = malloc(1);
  res->size                  = 0;

  headers = curl_slist_append(headers, auth);

  hnd = curl_easy_init();
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, NSS_OCTOPASS_VERSION_WITH_NAME);
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, res);

  result = curl_easy_perform(hnd);

  if (result != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
  } else {
    long *code;
    curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &code);
    if (con->syslog) {
      syslog(LOG_INFO, "http status: %ld -- %lu bytes retrieved", (long)code, (long)res->size);
    }
  }

  curl_easy_cleanup(hnd);
  curl_slist_free_all(headers);
}

void nss_octopass_github_request(struct config *con, char *url, struct response *res)
{
  if (NSS_OCTOPASS_CACHE == 0) {
    nss_octopass_github_request_without_cache(con, url, res);
    return;
  }

  char *base = curl_escape(url, strlen(url));
  char f[strlen(base) + strlen(con->token) + 6];
  char *file = f;
  sprintf(f, "tmp/cache/%s-%s", base, nss_octopass_truncate(con->token, 6));

  FILE *fp = fopen(file, "r");

  if (fp == NULL) {
    nss_octopass_github_request_without_cache(con, url, res);
    nss_octopass_export_file(file, res->data);
  } else {
    fclose(fp);
    res->data = (char *)nss_octopass_import_file(file);
    res->size = strlen(res->data);
  }
}

int nss_octopass_github_team_id(char *team, char *data)
{
  json_t *root;
  json_error_t error;
  root = json_loads(data, 0, &error);

  size_t i;
  for (i = 0; i < json_array_size(root); i++) {
    json_t *data  = json_array_get(root, i);
    const char *t = json_string_value(json_object_get(data, "name"));

    if (strcmp(team, t) == 0) {
      const json_int_t id = json_integer_value(json_object_get(data, "id"));
      json_decref(root);
      return id;
    }
  }

  json_decref(root);
  return 0;
}

json_t *nss_octopass_github_team_member_by_name(char *name, json_t *root)
{
  size_t i;
  json_t *data;

  for (i = 0; i < json_array_size(root); i++) {
    data          = json_array_get(root, i);
    const char *u = json_string_value(json_object_get(data, "login"));
    if (strcmp(name, u) == 0) {
      return data;
    }
  }

  return json_object();
}

json_t *nss_octopass_github_team_member_by_id(int gh_id, json_t *root)
{
  size_t i;
  json_t *data;

  for (i = 0; i < json_array_size(root); i++) {
    data                = json_array_get(root, i);
    const json_int_t id = json_integer_value(json_object_get(data, "id"));
    if (id == gh_id) {
      return data;
    }
  }

  return data;
}

int nss_octopass_github_id_by_name(char *name, char *res_body)
{
  json_t *root;
  json_error_t error;
  root = json_loads(res_body, 0, &error);

  json_t *data = nss_octopass_github_team_member_by_name(name, root);
  if (data) {
    const json_int_t id = json_integer_value(json_object_get(data, "id"));
    syslog(LOG_INFO, "github: user(%s) -> id(%d)", name, (int)id);
    json_decref(root);
    return id;
  }

  json_decref(root);
  return 0;
}

const char *nss_octopass_github_username_by_id(int id, char *res_body)
{
  json_t *root;
  json_error_t error;
  root = json_loads(res_body, 0, &error);

  json_t *data = nss_octopass_github_team_member_by_id(id, root);

  if (data) {
    const char *login = json_string_value(json_object_get(data, "login"));
    syslog(LOG_INFO, "github: id(%d) -> user(%s)", (int)id, login);

    const char *result = strdup(login);
    json_decref(root);

    return result;
  }

  json_decref(root);
  return NULL;
}

void nss_octopass_github_team_members_outoput(struct config *con, char *data)
{
  json_t *root;
  json_error_t error;
  root = json_loads(data, 0, &error);

  char *gname = con->group_name;

  size_t i;
  for (i = 0; i < json_array_size(root); i++) {
    json_t *data        = json_array_get(root, i);
    const json_int_t id = json_integer_value(json_object_get(data, "id"));
    const char *user    = json_string_value(json_object_get(data, "login"));
    int uid             = con->uid_starts + id;
    printf("uid=%d(%s) gid=%ld(%s) groups=%ld(%s)\n", uid, user, con->gid, gname, con->gid, gname);
  }

  json_decref(root);
}

int nss_octopass_team_id(struct config *con)
{
  char url[strlen(con->endpoint) + strlen(con->organization) + 64];
  sprintf(url, "%s/orgs/%s/teams", con->endpoint, con->organization);

  struct response res;
  nss_octopass_github_request(con, url, &res);

  if (!res.data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return -1;
  }

  return nss_octopass_github_team_id(con->team, res.data);
}

int nss_octopass_team_members_by_team_id(struct config *con, int team_id, struct response *res)
{
  char url[strlen(con->endpoint) + strlen(con->organization) + 64];
  sprintf(url, "%s/teams/%d/members", con->endpoint, team_id);

  nss_octopass_github_request(con, url, res);

  if (!res->data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return -1;
  }

  return 0;
}

int nss_octopass_team_members(struct config *con, struct response *res)
{
  int team_id = nss_octopass_team_id(con);
  if (team_id == -1) {
    return -1;
  }

  int status = nss_octopass_team_members_by_team_id(con, team_id, res);
  if (status == -1) {
    return -1;
  }

  return 0;
}

int nss_octopass_id_by_name(struct config *con, struct response *res, char *name)
{
  int status = nss_octopass_team_members(con, res);
  if (status == -1) {
    return -1;
  }

  return nss_octopass_github_id_by_name(name, res->data);
}

const char *nss_octopass_name_by_id(struct config *con, struct response *res, int id)
{
  int status = nss_octopass_team_members(con, res);
  if (status == -1) {
    return NULL;
  }

  return nss_octopass_github_username_by_id(id, res->data);
}
