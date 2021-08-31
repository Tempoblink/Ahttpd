#include "socket.h"
#include "log.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <unistd.h>

#define MAXSLEEP 128

int ahttpd_socket(int domain, int type, int protocol) {
    int ret;
    if ((ret = socket(domain, type, protocol)) < 0) {
        //log
        ahttpd_log(LOG_ERR, "socket failed. %s", strerror(errno));
    } else {
        ahttpd_log(LOG_INFO, "socket success.");
    }
    return ret;
}//ahttpd_socket
int ahttpd_bind(int sockfd, const struct sockaddr *addr, socklen_t len) {
    int ret;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if ((ret = bind(sockfd, addr, len)) < 0) {
        //log
        ahttpd_log(LOG_ERR, "bind failed. %s", strerror(errno));
    } else {
        ahttpd_log(LOG_INFO, "bind success.");
    }
    return ret;
}//ahttpd_bind
int ahttpd_connect(int sockfd, struct sockaddr *addr, socklen_t alenp) {
    int numsec;
    for (numsec = 1; numsec <= MAXSLEEP; numsec <<= 1) {
        if (connect(sockfd, addr, alenp) == 0) {
            ahttpd_log(LOG_INFO, "connect success.");
            return 0;
        }

        if (numsec <= MAXSLEEP / 2)
            sleep(numsec);
    }
    ahttpd_log(LOG_ERR, "connect failed. %s", strerror(errno));
    return -1;
}//ahttpd_connect
int ahttpd_listen(int sockfd, int backlog) {
    int ret;
    if ((ret = listen(sockfd, backlog)) < 0) {
        ahttpd_log(LOG_ERR, "listen failed. %s", strerror(errno));
    } else {
        ahttpd_log(LOG_INFO, "listen success.");
    }
    return ret;
}//ahttpd_listen
int ahttpd_accept(int sockfd, struct sockaddr *addr, socklen_t *len) {
    int ret;
    do {
        if ((ret = accept(sockfd, addr, len)) < 0) {
            if ((errno == ECONNABORTED) || (errno == EINTR)) {
                ahttpd_log(LOG_WARNING, "accept try again...");
                continue;
            } else {
                ahttpd_log(LOG_ERR, "accept failed. %s", strerror(errno));
            }
        } else {
            ahttpd_log(LOG_INFO, "accept success.");
        }
    } while (0);

    if (addr != NULL) {
        char client_ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &((struct sockaddr_in *) addr)->sin_addr.s_addr, client_ip, INET_ADDRSTRLEN) == NULL) {
            ahttpd_log(LOG_ERR, "client ip inet_ntop failed. %s", strerror(errno));
        } else {
            ahttpd_log(LOG_INFO, "[client: %s:%d]", client_ip, ntohs(((struct sockaddr_in *) addr)->sin_port));
        }
    }
    return ret;
}//ahttpd_accept
ssize_t ahttpd_read(int fd, void *buf, size_t nbytes) {
    ssize_t ret;
    do {
        if ((ret = read(fd, buf, nbytes)) == -1) {
            if (errno == EINTR)
                continue;
            else
                ahttpd_log(LOG_ERR, "read error. %s", strerror(errno));
        }
    } while (0);
    return ret;
}//ahttpd_read
ssize_t ahttpd_write(int fd, void *buf, size_t nbytes) {
    ssize_t ret;
    do {
        if ((ret = write(fd, buf, nbytes)) == -1) {
            if (errno == EINTR)
                continue;
            else
                ahttpd_log(LOG_ERR, "wrtie error. %s", strerror(errno));
        }
    } while (0);
    return ret;
}//ahttpd_write
int ahttpd_close(int fd) {
    int ret;
    if ((ret = close(fd)) == -1) {
        ahttpd_log(LOG_ERR, "close error. %s", strerror(errno));
    }
    return ret;
}//ahttpd_close
int ahttpd_setnoblock(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_flag | O_NONBLOCK);
    return old_flag;
}//ahttpd_setnoblock
