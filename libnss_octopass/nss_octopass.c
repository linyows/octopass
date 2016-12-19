#include "nss_octopass.h"

struct response {
    char *data;
    size_t pos;
};


// Newer versions of Jansson have this but the version
// on Ubuntu 12.04 don't, so make a wrapper.
extern size_t
j_strlen(json_t *str)
{
    return strlen(json_string_value(str));
}


static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct response *result = (struct response *)stream;
    size_t required_len = result->pos + size * nmemb;

    if(required_len >= NSS_OCTOPASS_INITIAL_BUFFER_SIZE - 1)
    {
        if (required_len < NSS_OCTOPASS_MAX_BUFFER_SIZE)
        {
            result->data = realloc(result->data, required_len);
            if (!result->data){
                // Failed to initialize a large enough buffer for the data.
                return 0;
            }
        } else {
            // Request data is too large.
            return 0;
        }
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}


char *
nss_octopass_request(const char *url)
{
    CURL *curl = NULL;
    CURLcode status;
    struct curl_slist *headers = NULL;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl) goto error;

    data = malloc(NSS_OCTOPASS_INITIAL_BUFFER_SIZE);
    if(!data) goto error;

    struct response write_result = { .data = data, .pos = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);

    headers = curl_slist_append(headers, "User-Agent: NSS-OCTOPASS");
    curl_easy_setopt(curl, CURLOPT_OCTOPASSHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0) goto error;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200) goto error;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    data[write_result.pos] = '\0';

    return data;

error:
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    if(headers)
        curl_slist_free_all(headers);
    curl_global_cleanup();

    return NULL;
}

