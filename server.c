#include "event.h"
#include "http.h"
#include "log.h"
#include "socket.h"
#include "threadpool.h"
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>

int main(int argc, char const *argv[]) {
    ahttpd_log_create(NULL, NULL);
    int sockfd = ahttpd_socket(AF_INET, SOCK_STREAM, 0);
    ahttpd_setnoblock(sockfd);
    struct sockaddr_in server_info;
    memset(&server_info, 0x00, sizeof(struct sockaddr_in));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(80);
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);

    ahttpd_bind(sockfd, (struct sockaddr *) &server_info, sizeof(server_info));
    ahttpd_listen(sockfd, 128);

    ahttpd_threadpool_create(10, 10, 128);
    ahttpd_event_handler(sockfd, 128, ahttpd_http_process);

    ahttpd_threadpool_destroy();
    return 0;
}
