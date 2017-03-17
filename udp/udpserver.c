#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <netdb.h>

#define BUF_SIZE   4096
#define CLIENT_NUM 5


int main(int argc, char **argv)
{
    int                sockfd = 0, ret, flag = 1, portno, recvlen = 0;
    fd_set             read_fds;
    socklen_t          len;
    struct hostent    *server;
    struct timeval     timeout;
    struct sockaddr_in server_addr;
    char              *hostname;
    char               recvBuf[BUF_SIZE];

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        goto L_ERROR;
    }

    hostname = argv[1];
    portno   = atoi(argv[2]);

    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        goto L_ERROR;
    }

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(portno);
    len                  = sizeof(struct sockaddr_in);

    /* Create endpoint */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket()");
        goto L_ERROR;
    }

    /* Set socket option */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0)
    {
        perror("setsockopt()");
        goto L_ERROR;
    }

    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt()");
        goto L_ERROR;
    }

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind()");
        goto L_ERROR;
    }

    printf("bind to %s:%u... \n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;
        ret             = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        switch (ret)
        {
            case -1:
                perror("select()");
                break;
            case 0:
                //printf("select timeout\n");
                continue;
            default:
                /* Service all sockets */
                if (FD_ISSET(sockfd, &read_fds))
                {
                    struct sockaddr_in client_addr;

                    memset(recvBuf, 0x00, sizeof(recvBuf));
                    recvlen = recvfrom(sockfd,recvBuf,sizeof(recvBuf),0, (struct sockaddr *)&client_addr, &len);
                    if (recvlen <= 0)
                    {
                        perror("recv");
                        goto L_ERROR;
                    }
                    printf("Receive(%d): len=[%d]\r\n", sockfd, recvlen);
                    printf("%s\n", recvBuf);
                    fflush(stdout);
                } // end of if
                break;
        }
    }             // end of while

L_ERROR:
    if (sockfd > 0)
    {
        close(sockfd);
    }

    return 0;
}
