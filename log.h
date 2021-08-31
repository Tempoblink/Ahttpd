#ifndef _LOG_H
#define _LOG_H
#include <syslog.h>

int ahttpd_log_create(char *access_filename, char *error_filename);

int ahttpd_log(int logtype, char *logfmt, ...);

#endif
