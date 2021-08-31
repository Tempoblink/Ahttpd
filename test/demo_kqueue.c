#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {
    struct kevent changes[128];
    struct kevent event[128];

    struct sockaddr_in server;
    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int flag = fcntl(sockfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flag);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (void *) 1, sizeof((void *) 1));
    int ret = bind(sockfd, (struct sockaddr *) &server, sizeof(server));

    listen(sockfd, 128);

    int kq = kqueue();
    EV_SET(&(changes[0]), sockfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &kq);
    kevent(kq, changes, 1, NULL, 0, NULL);
    printf("kq = %d\n", kq);
    printf("add sockfd\n");

    char buf[1024];
    int nready, client;
    while (1) {
        nready = kevent(kq, NULL, 0, event, 128, NULL);
        if (nready < 0) {
            perror("kevent error.");
            exit(-1);
        }
        for (int i = 0; i < nready; i++) {
            if (event[i].flags & EV_EOF) {
                printf("client close.\n");
                printf("data = %ld\n", event[i].data);
                close(event[i].ident);
                EV_SET(&(changes[0]), event[i].ident, 0, EV_DELETE, 0, 0, NULL);
                kevent(*(int *) (event[i].udata), changes, 1, NULL, 0, NULL);
                printf("delete event.\n");
            } else if (event[i].ident == sockfd) {
                int j = 0;
                for (int k = 0; k < event[i].data; k++) {
                    client = accept(event[k].ident, NULL, NULL);
                    if (client == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else {
                            perror("accept error.");
                            exit(-1);
                        }
                    }
                    EV_SET(&(changes[j++]), client, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, event[i].udata);
                }
                kevent(*(int *) event[i].udata, changes, j, NULL, 0, NULL);
            } else if (event[i].flags & EVFILT_READ) {
                printf("client has send data.\n");
                read(event[i].ident, buf, event[i].data);
                write(STDOUT_FILENO, buf, event[i].data);
                write(event[i].ident, buf, event[i].data);
            } else {
                printf("event[i].ident = %ld\n", event[i].ident);
                printf("event[i].filter = %d\n", event[i].filter);
                printf("event[i].flags = %d\n", event[i].flags);
                printf("event[i].fflags = %d\n", event[i].fflags);
                printf("event[i].data = %ld\n", event[i].data);
            }
        }
    }
}
