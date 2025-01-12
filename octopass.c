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

#include "octopass.h"

static size_t write_response_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize      = size * nmemb;
  struct response *res = (struct response *)userp;

  if (realsize > OCTOPASS_MAX_BUFFER_SIZE) {
    fprintf(stderr, "Response is too large\n");
    return 0;
  }

  res->data = realloc(res->data, res->size + realsize + 1);
  if (res->data == NULL) {
    // out of memory!
    fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(res->data[res->size]), contents, realsize);
  res->size += realsize;
  res->data[res->size] = 0;

  return realsize;
}

void octopass_remove_quotes(char *s)
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
  memmove(s, &s[i], strlen(s));
}

const char *octopass_truncate(const char *str, int len)
{
  char s[len + 1];
  strncpy(s, str, len);
  *(s + len) = '\0';
  char *res  = strdup(s);
  return res;
}

const char *octopass_masking(const char *token)
{
  int len = 5;
  char s[strlen(token) + 1];
  sprintf(s, "%s ************ REDACTED ************", octopass_truncate(token, len));
  char *mask = strdup(s);
  return mask;
}

const char *octopass_url_normalization(char *url)
{
  char *slash = strrchr(url, '/');

  // only if the URL has no slashes and does not end with a slash
  if (slash != NULL && strcmp(slash, "/") != 0) {
    size_t url_length = strlen(url);
    // add a trailing slash and terminator
    size_t new_length = url_length + 2;

    char *res = malloc(new_length);
    if (!res) {
      fprintf(stderr, "Memory allocation failed in octopass_url_normalization\n");
      exit(1);
    }

    // add URL and slash to dynamic buffer
    snprintf(res, new_length, "%s/", url);
    return res;
  }

  return url;
}

// Unatched: 0
// Matched: 1
int octopass_match(char *str, char *pattern, char **matched)
{
  regex_t re;
  regmatch_t pm;
  int res = regcomp(&re, pattern, REG_EXTENDED);
  if (res != 0) {
    regfree(&re);
    return 0;
  }

  int cnt = 0;
  int offset = 0;

  // try the first match with a regular expression
  res = regexec(&re, str + offset, 1, &pm, 0);
  if (res != 0) {
    regfree(&re);
    return 0;
  }

  while (res == 0) {
    // calculate match start and end positions
    // IMPORTANT: +1, -1 for getting the quotes inside
    int match_start = pm.rm_so + 1;
    int match_end = pm.rm_eo - 1;
    int match_len = match_end - match_start;

    char *match_word = malloc(match_len + 1);
    if (!match_word) {
      fprintf(stderr, "Memory allocation failed in octopass_match\n");
      regfree(&re);
      return cnt;
    }

    // copy for matched
    strncpy(match_word, str + offset + match_start, match_len);
    match_word[match_len] = '\0';

    // append matched
    matched[cnt] = match_word;
    cnt++;

    // update offset for next match
    offset += pm.rm_eo;

    // try next match
    res = regexec(&re, str + offset, 1, &pm, 0);
  }

  regfree(&re);
  return cnt;
}

void octopass_override_config_by_env(struct config *con)
{
  char *token = getenv("OCTOPASS_TOKEN");
  if (token) {
    sprintf(con->token, "%s", token);
  }

  char *endpoint = getenv("OCTOPASS_ENDPOINT");
  if (endpoint) {
    const char *url = octopass_url_normalization(endpoint);
    sprintf(con->endpoint, "%s", url);
  }

  char *org = getenv("OCTOPASS_ORGANIZATION");
  if (org) {
    sprintf(con->organization, "%s", org);
  }

  char *team = getenv("OCTOPASS_TEAM");
  if (team) {
    sprintf(con->team, "%s", team);
  }

  char *owner = getenv("OCTOPASS_OWNER");
  if (owner) {
    sprintf(con->owner, "%s", owner);
  }

  char *repository = getenv("OCTOPASS_REPOSITORY");
  if (repository) {
    sprintf(con->repository, "%s", repository);
  }

  char *permission = getenv("OCTOPASS_PERMISSION");
  if (permission) {
    sprintf(con->permission, "%s", permission);
  }
}

void octopass_config_loading(struct config *con, char *filename)
{
  memset(con->endpoint, '\0', sizeof(con->endpoint));
  memset(con->token, '\0', sizeof(con->token));
  memset(con->organization, '\0', sizeof(con->organization));
  memset(con->team, '\0', sizeof(con->team));
  memset(con->repository, '\0', sizeof(con->repository));
  memset(con->permission, '\0', sizeof(con->permission));
  memset(con->group_name, '\0', sizeof(con->group_name));
  memset(con->home, '\0', sizeof(con->home));
  memset(con->shell, '\0', sizeof(con->shell));
  con->uid_starts         = (long)2000;
  con->gid                = (long)2000;
  con->cache              = (long)500;
  con->syslog             = false;
  con->shared_users_count = 0;

  FILE *file = fopen(filename, "r");

  if (file == NULL) {
    fprintf(stderr, "Config not found: %s\n", filename);
    exit(1);
  }

  char *line = NULL;
  size_t len = 0;

  while (getline(&line, &len, file) != -1) {
    // delete line-feeds
    line[strcspn(line, "\r\n")] = '\0';

    // skip when empty
    if (strlen(line) == 0) {
      continue;
    }

    char *lasts;
    char *key   = strtok_r(line, DELIM, &lasts);
    char *value = strtok_r(NULL, DELIM, &lasts);

    if (strcmp(key, "SharedUsers") == 0 && strlen(lasts) > 0) {
      char v[strlen(value) + strlen(lasts)];
      sprintf(v, "%s %s", value, lasts);
      value = strdup(v);
    } else {
      octopass_remove_quotes(value);
    }

    if (strcmp(key, "Endpoint") == 0) {
      const char *url = octopass_url_normalization(value);
      strncpy(con->endpoint, url, sizeof(con->endpoint) - 1);
    } else if (strcmp(key, "Token") == 0) {
      strncpy(con->token, value, sizeof(con->token) - 1);
    } else if (strcmp(key, "Organization") == 0) {
     strncpy(con->organization, value, sizeof(con->organization) - 1);
    } else if (strcmp(key, "Team") == 0) {
      strncpy(con->team, value, sizeof(con->team) - 1);
    } else if (strcmp(key, "Owner") == 0) {
      strncpy(con->owner, value, sizeof(con->owner) - 1);
    } else if (strcmp(key, "Repository") == 0) {
      strncpy(con->repository, value, sizeof(con->repository) - 1);
    } else if (strcmp(key, "Permission") == 0) {
      strncpy(con->permission, value, sizeof(con->permission) - 1);
    } else if (strcmp(key, "Group") == 0) {
      strncpy(con->group_name, value, sizeof(con->group_name) - 1);
    } else if (strcmp(key, "Home") == 0) {
      strncpy(con->home, value, sizeof(con->home) - 1);
    } else if (strcmp(key, "Shell") == 0) {
      strncpy(con->shell, value, sizeof(con->shell) - 1);
    } else if (strcmp(key, "UidStarts") == 0) {
      con->uid_starts = atoi(value);
    } else if (strcmp(key, "Gid") == 0) {
      con->gid = atoi(value);
    } else if (strcmp(key, "Cache") == 0) {
      con->cache = (long)atoi(value);
    } else if (strcmp(key, "Syslog") == 0) {
      con->syslog = (strcmp(value, "true") == 0);
    } else if (strcmp(key, "SharedUsers") == 0) {
      char *pattern = "\"([A-z0-9_-]+)\"";
      con->shared_users = calloc(MAXBUF, sizeof(char *));
      con->shared_users_count = octopass_match(value, pattern, con->shared_users);
      free(value);
    }
  }

  free(line);
  fclose(file);

  octopass_override_config_by_env(con);

  if (strlen(con->endpoint) == 0) {
    strncpy(con->endpoint, OCTOPASS_API_ENDPOINT, sizeof(con->endpoint) - 1);
  }
  if (strlen(con->group_name) == 0) {
    if (strlen(con->repository) != 0) {
      strncpy(con->group_name, con->repository, sizeof(con->group_name) - 1);
    } else {
      strncpy(con->group_name, con->team, sizeof(con->group_name) - 1);
    }
  }
  if (strlen(con->owner) == 0 && strlen(con->organization) != 0) {
    strncpy(con->owner, con->organization, sizeof(con->owner) - 1);
  }
  if (strlen(con->repository) != 0 && strlen(con->permission) == 0) {
    strncpy(con->permission, "write", sizeof(con->permission) - 1);
  }
  if (strlen(con->home) == 0) {
    strncpy(con->home, "/home/%s", sizeof(con->home) - 1);
  }
  if (strlen(con->shell) == 0) {
    strncpy(con->shell, "/bin/bash", sizeof(con->shell) - 1);
  }

  if (con->syslog) {
    const char *pg_name = "octopass";
    openlog(pg_name, LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "config {endpoint: %s, token: %s, organization: %s, team: %s, owner: %s, repository: %s, permission: %s "
                     "syslog: %d, uid_starts: %ld, gid: %ld, group_name: %s, home: %s, shell: %s, cache: %ld}",
      con->endpoint, octopass_masking(con->token), con->organization, con->team, con->owner, con->repository, con->permission,
      con->syslog, con->uid_starts, con->gid, con->group_name, con->home, con->shell, con->cache);
  }
}

void octopass_export_file(struct config *con, char *dir, char *file, char *data)
{
  struct stat statbuf;
  if (stat(dir, &statbuf) != 0) {
    mode_t um = {0};
    um        = umask(0);
    mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
    umask(um);
  }

  FILE *fp = fopen(file, "w");
  if (!fp) {
    if (con->syslog) {
      syslog(LOG_ERR, "File open failure: %s %s", file, strerror(errno));
    } else {
      fprintf(stderr, "File open failure: %s %s\n", file, strerror(errno));
    }
    exit(1);
    return;
  }
  fprintf(fp, "%s", data);

  mode_t um = {0};
  um        = umask(0);
  fchmod(fileno(fp), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
  umask(um);

  fclose(fp);
}

const char *octopass_import_file(struct config *con, char *file)
{
  FILE *fp = fopen(file, "r");
  if (!fp) {
    if (con->syslog) {
      syslog(LOG_ERR, "File open failure: %s %s", file, strerror(errno));
    } else {
      fprintf(stderr, "File open failure: %s %s\n", file, strerror(errno));
    }
    exit(1);
  }

  char *data = NULL;
  size_t data_size = 0;
  size_t data_capacity = MAXBUF;

  data = malloc(data_capacity);
  if (!data) {
    fprintf(stderr, "Malloc failed\n");
    fclose(fp);
    return NULL;
  }
  data[0] = '\0';

  char *line = NULL;
  size_t line_len = 0;

  while (getline(&line, &line_len, fp) != -1) {
    size_t line_size = strlen(line);

    // Expand buffers when over data_capacity
    if (data_size + line_size + 1 > data_capacity) {
      data_capacity *= 2;
      char *new_data = realloc(data, data_capacity);
      if (!new_data) {
        fprintf(stderr, "Realloc failed\n");
        free(data);
        free(line);
        fclose(fp);
        return NULL;
      }
      data = new_data;
    }

    // append a row to data
    strcat(data, line);
    data_size += line_size;
  }

  free(line);
  fclose(fp);

  // return copy
  const char *res = strdup(data);
  free(data);

  return res;
}

void octopass_github_request_without_cache(struct config *con, char *url, struct response *res, char *token)
{
  if (!con || !url || !res) {
    fprintf(stderr, "Invalid arguments passed to octopass_github_request_without_cache\n");
    return;
  }

  if (con->syslog) {
    syslog(LOG_INFO, "http get -- %s", url);
  }

  size_t auth_size = snprintf(NULL, 0, "Authorization: token %s", token ? token : con->token) + 1;
  char *auth = malloc(auth_size);
  if (!auth) {
    fprintf(stderr, "Memory allocation failed for auth header\n");
    return;
  }
  snprintf(auth, auth_size, "Authorization: token %s", token ? token : con->token);

  CURL *hnd = curl_easy_init();
  if (!hnd) {
    fprintf(stderr, "Failed to initialize cURL\n");
    free(auth);
    return;
  }

  CURLcode result;
  struct curl_slist *headers = NULL;
  res->data                  = malloc(1);
  res->size                  = 0;
  res->httpstatus            = (long *)0;

  headers = curl_slist_append(headers, auth);
  if (!headers) {
    fprintf(stderr, "Failed to set cURL headers\n");
    curl_easy_cleanup(hnd);
    free(auth);
    return;
  }

  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, OCTOPASS_VERSION_WITH_NAME);
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, res);

  result = curl_easy_perform(hnd);

  if (result != CURLE_OK) {
    if (con->syslog) {
      syslog(LOG_ERR, "cURL failed: %s", curl_easy_strerror(result));
    } else {
      fprintf(stderr, "cURL failed: %s\n", curl_easy_strerror(result));
    }
  } else {
    long *code;
    curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &code);
    res->httpstatus = code;
    if (con->syslog) {
      syslog(LOG_INFO, "http status: %ld -- %lu bytes retrieved", (long)code, (long)res->size);
    }
  }

  // clean-up
  curl_easy_cleanup(hnd);
  curl_slist_free_all(headers);
  free(auth);

  if (!res->data) {
    res->data = malloc(1);
    if (res->data) {
      res->data[0] = '\0';
    }
  }
}

void octopass_github_request(struct config *con, char *url, struct response *res)
{
  char *token = NULL;
  if (con->cache == 0) {
    octopass_github_request_without_cache(con, url, res, token);
    return;
  }

  char *base = curl_escape(url, strlen(url));
  if (!base) {
    fprintf(stderr, "Failed to escape URL\n");
    exit(1);
  }

  // create dir path with dynamic buffers
  size_t dpath_size = snprintf(NULL, 0, "%s/%d", OCTOPASS_CACHE_DIR, geteuid()) + 1;
  char *dpath = malloc(dpath_size);
  if (!dpath) {
    fprintf(stderr, "Memory allocation failed for dpath\n");
    curl_free(base);
    exit(1);
  }
  snprintf(dpath, dpath_size, "%s/%d", OCTOPASS_CACHE_DIR, geteuid());

  // create file path with dynamic buffers
  size_t fpath_size = snprintf(NULL, 0, "%s/%s-%s", dpath, base, octopass_truncate(con->token, 6)) + 1;
  char *fpath = malloc(fpath_size);
  if (!fpath) {
    fprintf(stderr, "Memory allocation failed for fpath\n");
    free(dpath);
    curl_free(base);
    exit(1);
  }
  snprintf(fpath, fpath_size, "%s/%s-%s", dpath, base, octopass_truncate(con->token, 6));

  curl_free(base);
  FILE *fp = fopen(fpath, "r");
  long *ok_code = (long *)200;

  if (fp == NULL) {
    octopass_github_request_without_cache(con, url, res, token);
    if (res->httpstatus == ok_code) {
      octopass_export_file(con, dpath, fpath, res->data);
    }
  } else {
    fclose(fp);

    struct stat statbuf;
    if (stat(fpath, &statbuf) != -1) {
      unsigned long now = time(NULL);
      unsigned long diff = now - statbuf.st_mtime;
      if (diff > con->cache) {
        octopass_github_request_without_cache(con, url, res, token);
        if (res->httpstatus == ok_code) {
          octopass_export_file(con, dpath, fpath, res->data);
        }
        free(dpath);
        free(fpath);
        return;
      }
    }

    if (con->syslog) {
      syslog(LOG_INFO, "use cache: %s", fpath);
    }

    res->data = (char *)octopass_import_file(con, fpath);
    res->size = strlen(res->data);
  }

  free(dpath);
  free(fpath);
}

int octopass_github_team_id(char *team_name, char *data)
{
  json_error_t error;
  json_t *teams = json_loads(data, 0, &error);
  json_t *team;
  int i;

  json_array_foreach(teams, i, team)
  {
    if (!json_is_object(team)) {
      continue;
    }
    const char *name = json_string_value(json_object_get(team, "name"));
    if (name != NULL && strcmp(team_name, name) == 0) {
      const json_int_t id = json_integer_value(json_object_get(team, "id"));
      return id;
    }
    const char *slug = json_string_value(json_object_get(team, "slug"));
    if (name != NULL && strcmp(team_name, slug) == 0) {
      const json_int_t id = json_integer_value(json_object_get(team, "id"));
      return id;
    }
  }

  return -1;
}

json_t *octopass_github_team_member_by_name(char *name, json_t *members)
{
  json_t *member;
  int i;

  json_array_foreach(members, i, member)
  {
    const char *u = json_string_value(json_object_get(member, "login"));
    if (strcmp(name, u) == 0) {
      return member;
    }
  }

  return json_object();
}

json_t *octopass_github_team_member_by_id(int gh_id, json_t *members)
{
  json_t *member;
  int i;

  json_array_foreach(members, i, member)
  {
    const json_int_t id = json_integer_value(json_object_get(member, "id"));
    if (id == gh_id) {
      return member;
    }
  }

  return json_object();
}

int octopass_team_id(struct config *con)
{
  size_t url_size = snprintf(NULL, 0, OCTOPASS_TEAMS_URL, con->endpoint, con->organization) + 1;
  char *url = malloc(url_size);
  if (!url) {
    fprintf(stderr, "Memory allocation failed for teams URL\n");
    return -1;
  }
  snprintf(url, url_size, OCTOPASS_TEAMS_URL, con->endpoint, con->organization);

  struct response res;
  octopass_github_request(con, url, &res);

  if (!res.data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return -1;
  }

  int id = octopass_github_team_id(con->team, res.data);
  free(res.data);

  return id;
}

int octopass_team_members_by_team_id(struct config *con, int team_id, struct response *res)
{
  size_t url_size = snprintf(NULL, 0, OCTOPASS_TEAMS_MEMBERS_URL, con->endpoint, team_id) + 1;
  char *url = malloc(url_size);
  if (!url) {
    fprintf(stderr, "Memory allocation failed for team members URL\n");
    return -1;
  }
  snprintf(url, url_size, OCTOPASS_TEAMS_MEMBERS_URL, con->endpoint, team_id);

  octopass_github_request(con, url, res);

  if (!res->data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return -1;
  }

  return 0;
}

int octopass_team_members(struct config *con, struct response *res)
{
  int team_id = octopass_team_id(con);
  if (team_id == -1) {
    if (con->syslog) {
      syslog(LOG_INFO, "team not found: %s", con->team);
    }
    return -1;
  }

  int status = octopass_team_members_by_team_id(con, team_id, res);
  if (status == -1) {
    return -1;
  }

  return 0;
}

char *octopass_permission_level(char *permission)
{
  char *level;

  if (strcmp(permission, "admin") == 0) {
    level = "admin";
  } else if (strcmp(permission, "write") == 0) {
    level = "push";
  } else if (strcmp(permission, "read") == 0) {
    level = "pull";
  } else {
    fprintf(stderr, "Unknown permission: %s\n", permission);
  }

  return level;
}

int octopass_is_authorized_collaborator(struct config *con, json_t *collaborator)
{
  if (!json_is_object(collaborator)) {
    return 0;
  }

  json_t *permissions = json_object_get(collaborator, "permissions");
  if (!json_is_object(permissions)) {
    return 0;
  }

  char *level = octopass_permission_level(con->permission);
  json_t *permission = json_object_get(permissions, level);
  return json_is_true(permission) ? 1 : 0;
}

int octopass_rebuild_data_with_authorized(struct config *con, struct response *res)
{
  json_error_t error;
  json_t *collaborators = json_loads(res->data, 0, &error);
  json_t *collaborator;
  json_t *new_data = json_array();
  int i;

  json_array_foreach(collaborators, i, collaborator)
  {
    if (1 == octopass_is_authorized_collaborator(con, collaborator)) {
      json_array_append_new(new_data, collaborator);
    }
  }
  res->data = json_dumps(new_data, 0);

  return 0;
}

int octopass_repository_collaborators(struct config *con, struct response *res)
{
  size_t url_size = snprintf(NULL, 0, OCTOPASS_COLLABORATORS_URL, con->endpoint, con->owner, con->repository) + 1;
  char *url = malloc(url_size);
  if (!url) {
    fprintf(stderr, "Memory allocation failed for collaborators URL\n");
    return -1;
  }
  snprintf(url, url_size, OCTOPASS_COLLABORATORS_URL, con->endpoint, con->owner, con->repository);

  octopass_github_request(con, url, res);

  if (!res->data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return -1;
  }

  return octopass_rebuild_data_with_authorized(con, res);
}

int octopass_members(struct config *con, struct response *res)
{
  if (strlen(con->repository) != 0) {
    return octopass_repository_collaborators(con, res);
  } else {
    return octopass_team_members(con, res);
  }
}

// OK: 0
// NG: 1
int octopass_autentication_with_token(struct config *con, char *user, char *token)
{
  size_t url_size = snprintf(NULL, 0, OCTOPASS_USER_URL, con->endpoint) + 1;
  char *url = malloc(url_size);
  if (!url) {
    fprintf(stderr, "Memory allocation failed for user URL\n");
    return 1;
  }
  snprintf(url, url_size, OCTOPASS_USER_URL, con->endpoint);

  struct response res;
  octopass_github_request_without_cache(con, url, &res, token);

  long *ok_code = (long *)200;
  if (res.httpstatus == ok_code) {
    json_t *root;
    json_error_t error;
    root              = json_loads(res.data, 0, &error);
    const char *login = json_string_value(json_object_get(root, "login"));

    if (strcmp(login, user) == 0) {
      return 0;
    }
  }

  if (con->syslog) {
    closelog();
  }
  return 1;
}

const char *octopass_only_keys(char *data)
{
  json_t *root;
  json_error_t error;
  root = json_loads(data, 0, &error);

  char *keys = calloc(OCTOPASS_MAX_BUFFER_SIZE, sizeof(char *));

  size_t i;
  for (i = 0; i < json_array_size(root); i++) {
    json_t *obj     = json_array_get(root, i);
    const char *key = json_string_value(json_object_get(obj, "key"));
    strcat(keys, strdup(key));
    strcat(keys, "\n");
  }

  const char *res = strdup(keys);
  free(keys);

  return res;
}

const char *octopass_github_user_keys(struct config *con, char *user)
{
  size_t url_size = snprintf(NULL, 0, OCTOPASS_USERS_KEYS_URL, con->endpoint, user) + 1;
  char *url = malloc(url_size);
  if (!url) {
    fprintf(stderr, "Memory allocation failed for user keys URL\n");
    return NULL;
  }
  snprintf(url, url_size, OCTOPASS_USERS_KEYS_URL, con->endpoint, user);

  struct response res;
  octopass_github_request(con, url, &res);

  if (!res.data) {
    fprintf(stderr, "Request failure\n");
    if (con->syslog) {
      closelog();
    }
    return NULL;
  }

  return octopass_only_keys(res.data);
}

const char *octopass_github_team_members_keys(struct config *con)
{
  json_t *root;
  json_error_t error;

  struct response res;
  int status = octopass_team_members(con, &res);

  if (status != 0) {
    free(res.data);
    return NULL;
  }
  root = json_loads(res.data, 0, &error);
  free(res.data);

  if (!json_is_array(root)) {
    return NULL;
  }

  char *members_keys = calloc(OCTOPASS_MAX_BUFFER_SIZE, sizeof(char *));
  size_t cnt         = json_array_size(root);
  int i              = 0;

  for (i = 0; i < cnt; i++) {
    json_t *j_obj = json_array_get(root, i);
    if (!json_is_object(j_obj)) {
      continue;
    }
    json_t *j_name = json_object_get(j_obj, "login");
    if (!json_is_string(j_name)) {
      continue;
    }

    const char *login = json_string_value(j_name);
    const char *keys  = octopass_github_user_keys(con, (char *)login);
    strcat(members_keys, strdup(keys));
  }

  const char *result = strdup(members_keys);
  free(members_keys);

  return (strlen(result) > 0) ? result : NULL;
}
