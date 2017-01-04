#ifndef NSS_OCTOPASS_H
#define NSS_OCTOPASS_H

#include <curl/curl.h>
#include <errno.h>
#include <grp.h>
#include <jansson.h>
#include <nss.h>
#include <pthread.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>

//#include <unistd.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/ioctl.h>
//#include <netinet/in.h>
//#include <net/if.h>
//#include <arpa/inet.h>

#define NSS_OCTOPASS_VERSION "0.1.0"
#define NSS_OCTOPASS_VERSION_WITH_NAME "nss-octopass/" NSS_OCTOPASS_VERSION
#define NSS_OCTOPASS_CONFIG_FILE "/octopass/octopass.conf"
//#define NSS_OCTOPASS_CONFIG_FILE "/etc/octopass.conf"
#define NSS_OCTOPASS_LOCK() do { pthread_mutex_lock(&NSS_OCTOPASS_MUTEX); } while (0)
#define NSS_OCTOPASS_UNLOCK() do { pthread_mutex_unlock(&NSS_OCTOPASS_MUTEX); } while (0)
#ifndef NSS_OCTOPASS_SCRIPT
#define NSS_OCTOPASS_SCRIPT "/sbin/nss-octopass"
#endif

// 256KB
#define NSS_OCTOPASS_INITIAL_BUFFER_SIZE (256 * 1024)
// 10MB
#define NSS_OCTOPASS_MAX_BUFFER_SIZE (10 * 1024 * 1024)

#define MAXBUF 1024
#define DELIM " = "

extern int nss_octopass_request(char *res_body);

#endif /* NSS_OCTOPASS_H */
