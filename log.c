#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>


#define DEFAULT_LOG_FILE "/var/log/AHttpd.log"
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define PROCESS_NAME "Ahttpd"
#define DATEFMT "[%m-%d %H:%M:%S]"
#define TIME_BUF_SIZE 64

static FILE *access_log = NULL, *error_log = NULL;

char *_ahttpd_log_level(int logtype) {
    switch (logtype) {
        case LOG_INFO:
            return "INFO";
        case LOG_WARNING:
            return "WARN";
        case LOG_DEBUG:
            return "DEBUG";
        case LOG_ERR:
            return "ERROR";
        default:
            return "UNKNOW";
    }
}//_ahttpd_log_level
int _ahttpd_log_mv(char *filename) {
    int status, len, i = 1;
    char new_filename[PATH_MAX];
    if ((len = snprintf(new_filename, PATH_MAX, "%s.", filename)) < 0) {
        //log
        return -1;
    }
    do {
        new_filename[len] = '0' + i++;
        new_filename[len + 1] = '\0';
        status = link(filename, new_filename);
    } while (status != 0);
    unlink(filename);
    return 0;
}//_ahttpd_log_mv
int _ahttpd_log_open(char *filename) {
    int status;
    if (filename == NULL) {
        filename = DEFAULT_LOG_FILE;
    }
    if (strlen(filename) > PATH_MAX) {
        errno = ENAMETOOLONG;
        exit(1);
    }
    do {
        status = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (status < 0) {
            if (errno == EEXIST) {
                _ahttpd_log_mv(filename);
            } else {
                //log
                exit(-1);
            }
        }
    } while (status < 0);
    return status;
}//_ahttpd_log_open
int ahttpd_log(int logtype, char *logfmt, ...) {
    int len;
    time_t now_time;
    FILE *log_file = access_log;
    if (logtype == LOG_ERR)
        log_file = error_log;

    va_list ap;
    char timebuf[TIME_BUF_SIZE];
    time(&now_time);
    memset(timebuf, 0x00, TIME_BUF_SIZE);
    strftime(timebuf, TIME_BUF_SIZE, DATEFMT, localtime(&now_time));
    printf("%s[%-6s][process:%s][pid:%-6d]", timebuf, _ahttpd_log_level(logtype), PROCESS_NAME, getpid());
    va_start(ap, logfmt);
    vprintf(logfmt, ap);
    va_end(ap);
    putchar('\n');
    return 0;
}//ahttpd_log
void _ahttpd_log_close(void) {
    int status, times;
    unsigned short file_type;
    if (access_log) {
        times = 0;
        do {
            status = fclose(access_log);
            if (status == EOF) {
                ahttpd_log(LOG_ERR, "access_log close failed. %s", strerror(errno));
                if (times++ < 5) {
                    ahttpd_log(LOG_ERR, "try to close access_log again.");
                    continue;
                } else {
                    file_type = 0;
                    goto errout;
                }
            }
        } while (0);
    }
    if (error_log) {
        if ((status = fclose(error_log)) == EOF) {
            file_type = 1;
            goto errout;
        }
    }
errout:
    openlog(PROCESS_NAME, LOG_PID, LOG_USER);
    syslog(LOG_ERR, "program crash. %s close failed. %s", file_type ? "error_log" : "access_log", strerror(errno));
    _exit(-1);
}//_ahttpd_log_close
int ahttpd_log_create(char *access_filename, char *error_filename) {
    int file_type, access_fd, error_fd;

    if (access_filename) {
        file_type = 0;
        if ((access_fd = _ahttpd_log_open(access_filename)) < 0) goto errout;
        if (dup2(access_fd, STDOUT_FILENO) < 0) goto errout;
        close(access_fd);
    }
    access_log = stdout;
    if (error_filename) {
        file_type = 1;
        if ((error_fd = _ahttpd_log_open(error_filename)) < 0) goto errout;
        if (dup2(error_fd, STDERR_FILENO) < 0) goto errout;
        close(error_fd);
    }
    error_log = stderr;

    if (atexit(_ahttpd_log_close) != 0) {
        syslog(LOG_ERR, "program crash. atexit close failed. %s", strerror(errno));
        _exit(-1);
    }
    return 0;

errout:
    openlog(PROCESS_NAME, LOG_PID, LOG_USER);
    syslog(LOG_ERR, "program crash. %s fdopen failed. %s", file_type ? "error_log" : "access_log", strerror(errno));
    _exit(-1);
}//ahttpd_log_create
