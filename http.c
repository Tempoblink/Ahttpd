#include "event.h"
#include "log.h"
#include "socket.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>

#define DEFAULT_SERVER_NAME "Ahttpd/1.0.0"
#define DEFAULT_HTTP_VERSION "HTTP/1.0"
#define DEFAULT_WEB_DIR "httpdoc"

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_UNKNOW
} HTTP_METHOD;

typedef enum {
    HTTP_VERSION_1_0 = 0,
    HTTP_VERSION_UNKNOW
} HTTP_VERSION;

typedef enum {
    RESOURCE_SUCESS = 0,
    RESOURCE_NO_FOUND,
    RESOURCE_ACCESS_FORBIDDEN
} RESOURCE;

#ifndef MAX_NAME
#define MAX_NAME 1024
#endif

typedef struct http_request {
    HTTP_METHOD method;
    RESOURCE file_stat;
    HTTP_VERSION version;
    char path[MAX_NAME];
    struct iovec *msg;
} http_request;

void _ahttpd_check_request(int fd, http_request *req) {
    char buf[1024], method[16], version[16];
    int len = sprintf(req->path, DEFAULT_WEB_DIR);
    int ret = ahttpd_read(fd, buf, sizeof(buf));
    if (sscanf(buf, "%[^ ] %[^ ] %[^\r\n]", method, req->path + len, version) != EOF) {
        if (strcasecmp(method, "GET") == 0) {
            req->method = HTTP_METHOD_GET;
        } else {
            req->method = HTTP_METHOD_UNKNOW;
        }
        if (strcasecmp(version, "HTTP/1.0") == 0) {
            req->version = HTTP_VERSION_1_0;
        } else {
            req->version = HTTP_VERSION_UNKNOW;
        }
    } else {
        req->method = HTTP_METHOD_UNKNOW;
    }
}//_ahttpd_check_request
void _ahttpd_open_file(http_request *req) {
    struct stat file_stat;
    if (req->method == HTTP_METHOD_UNKNOW || req->version == HTTP_VERSION_UNKNOW) {
        sprintf(req->path, "%s/400.html", DEFAULT_WEB_DIR);
    }
    do {
        if (stat(req->path, &file_stat) < 0) {
            req->file_stat = RESOURCE_NO_FOUND;
            sprintf(req->path, "%s/404.html", DEFAULT_WEB_DIR);
            continue;
        }
        if (!(file_stat.st_mode & S_IROTH)) {
            req->file_stat = RESOURCE_ACCESS_FORBIDDEN;
            sprintf(req->path, "%s/403.html", DEFAULT_WEB_DIR);
            continue;
        }
        if (S_ISDIR(file_stat.st_mode)) {
            strcat(req->path, "index.html");
            continue;
        }
    } while (0);
    (req->msg)[1].iov_len = file_stat.st_size;
    int fd = open(req->path, O_RDONLY);
    (req->msg)[1].iov_base = (void *) mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ahttpd_close(fd);
    req->file_stat = RESOURCE_SUCESS;
}//_ahttpd_open_file
void _ahttpd_make_response(int fd, http_request *req) {
    if (req->method == HTTP_METHOD_UNKNOW) {
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "%s %s %s\r\n", DEFAULT_HTTP_VERSION, "400", "Bad Request");
    }
    if (req->file_stat == RESOURCE_NO_FOUND) {
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "%s %s %s\r\n", DEFAULT_HTTP_VERSION, "404", "Not Found");
    }
    if (req->file_stat == RESOURCE_ACCESS_FORBIDDEN) {
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "%s %s %s\r\n", DEFAULT_HTTP_VERSION, "403", "Forbidden");
    }
    if (req->method == HTTP_METHOD_GET && req->version == HTTP_VERSION_1_0) {
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "%s %s %s\r\n", DEFAULT_HTTP_VERSION, "200", "OK");
    }
    req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "Server: %s\r\n", DEFAULT_SERVER_NAME);
    if (req->msg[1].iov_len > 0) {
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "Content-Type: %s\r\n", "text/html");
        req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "Content-Length: %ld\r\n", req->msg[1].iov_len);
    }
    req->msg[0].iov_len += sprintf((char *) (req->msg[0].iov_base) + req->msg[0].iov_len, "\r\n");
    if (writev(fd, req->msg, 2) < 0) {
        ahttpd_log(LOG_ERR, "writev error, %s", strerror(errno));
    }
    munmap(req->msg[1].iov_base, req->msg[1].iov_len);
}//_ahttpd_make_response
void ahttpd_http_process(void *arg) {
    struct kevent *cfd = (struct kevent *) arg;
    http_request *req = (http_request *) malloc(sizeof(http_request));
    memset(req, 0x00, sizeof(http_request));
    char header_buf[1024];
    memset(req, 0x00, sizeof(header_buf));
    req->msg = (struct iovec *) malloc(sizeof(struct iovec) * 2);
    req->msg[0].iov_base = header_buf;
    req->msg[0].iov_len = 0;
    req->msg[1].iov_len = 0;
    _ahttpd_check_request(cfd->ident, req);
    _ahttpd_open_file(req);
    _ahttpd_make_response(cfd->ident, req);
    free(req->msg);
    free(req);
}//ahttpd_http_process
