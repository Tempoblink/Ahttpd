#ifndef _SOCKET_H
#define _SOCKET_H
#include <sys/socket.h>

int ahttpd_socket(int domain, int type, int protocol);

int ahttpd_bind(int sockfd, const struct sockaddr *addr, socklen_t len);

int ahttpd_connect(int sockfd, struct sockaddr *addr, socklen_t alenp);

int ahttpd_listen(int sockfd, int backlog);

int ahttpd_accept(int sockfd, struct sockaddr *addr, socklen_t *len);

ssize_t ahttpd_read(int fd, void *buf, size_t nbytes);

ssize_t ahttpd_write(int fd, void *buf, size_t nbytes);

int ahttpd_close(int fd);

int ahttpd_setnoblock(int fd);
#endif
