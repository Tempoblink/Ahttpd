#include "../event.h"
#include "../log.h"
#include "../socket.h"
#include "../threadpool.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/socket.h>

void test(void *arg) {
    struct kevent *cfd = (struct kevent *) arg;
    char buf[1024];
    int len = ahttpd_read(cfd->ident, buf, cfd->data);
    for (int i = 0; i < len; ++i)
        buf[i] = toupper(buf[i]);
    ahttpd_write(cfd->ident, buf, len);
}

int main(int argc, char const *argv[]) {
    ahttpd_log_create("access.log", "error.log");
    ahttpd_threadpool_create(5, 15, 1024);

    int sockfd = ahttpd_socket(AF_INET, SOCK_STREAM, 0);
    int flag = fcntl(sockfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flag);
    struct sockaddr_in server;
    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(80);
    ahttpd_bind(sockfd, (struct sockaddr *) &server, sizeof(server));
    ahttpd_listen(sockfd, 256);
    ahttpd_event_handler(sockfd, 1024, test);
    ahttpd_threadpool_destroy();
    return 0;
}
