#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printf("usage:%s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        printf("connection failed\n");
    }
    else
    {
        const char *normal_data = "abc";
        sleep(5);
        send(sockfd, normal_data, strlen(normal_data), 0);
        char buf[128];
        while (1)
        {
            int rc = read(sockfd, buf, 128);
            if (rc == 0)
            {
                printf("server closed the connection: %s\n", strerror(errno));
                break;
            }
            else if (rc < 0)
            {
                if (errno == EINTR)
                {
                    printf("read interupted by signal, continue..\n");
                    continue;
                }
                printf("error reading from server.\n");
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}
