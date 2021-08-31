#include "../log.h"
#include "../socket.h"
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {
    ahttpd_log_create(NULL, NULL);

    int sockfd = ahttpd_socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_info;
    memset(&server_info, 0x00, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(80);
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = ahttpd_bind(sockfd, (struct sockaddr *) &server_info, sizeof(server_info));

    ret = ahttpd_listen(sockfd, 128);

    struct sockaddr_in client_info;
    memset(&client_info, 0x00, sizeof(client_info));
    socklen_t len = sizeof(client_info);
    int cfd = ahttpd_accept(sockfd, (struct sockaddr *) &client_info, &len);

    char buf[1024];
    if (ahttpd_read(cfd, buf, sizeof(buf))) {
        ahttpd_write(cfd, "Hello, World.\n", 14);
    }

    return 0;
}
