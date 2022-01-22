#define _GNU_SOURCE
#include <libgen.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char* argv[])
{
    if (argc <= 3)
    {
        printf("usage: %s ip_address port_number protocol(0 for tcp, 1 for udp)\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int protocol = atoi(argv[3]);
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);
    int sockfd = socket(PF_INET, protocol ? SOCK_DGRAM : SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
        printf("connection failed\n");
    }
    else
    {
        const char* oob_data = "hello from client";
        send(sockfd, oob_data, strlen(oob_data), 0);
        char buf[128] = {0};
        if (read(sockfd, buf, 128) > 0)
        {
            printf("From server: %s\n", buf);
        }
    }
    close(sockfd);
    return 0;
}
