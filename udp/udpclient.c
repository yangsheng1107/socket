/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 2048

char testStr[] = "Test Line 01: \r\n"
              "Test Line 02: \r\n"
              "Test Line 03: \r\n"
              "Test Line 04: \r\n"
              "Test Line 05: \r\n"
              "Test Line 06: \r\n"
              "Test Line 07: \r\n"
              "Test Line 08: \r\n"
              "Test Line 09: \r\n"
              "Test Line 10: \r\n"
              "Test Line 11: \r\n"
              "Test Line 12: \r\n"
              "Test Line 13: \r\n"
              "Test Line 14: \r\n";

int main(int argc, char **argv) {
    int sockfd = 0, portno, n, flag = 1, sendLen = 0;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, *line = NULL;
    char sendBuf[BUFSIZE], sourceBuf[BUFSIZE];
    struct timeval     timeout;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        goto L_ERROR;
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        goto L_ERROR;
    }

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        goto L_ERROR;
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* Set socket option */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0)
    {
        perror("setsockopt()");
        goto L_ERROR;
    }

    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt()");
        goto L_ERROR;
    }

    while (1)
    {
        sendLen = 0;
        strcpy(sourceBuf, testStr);
        line = strtok(sourceBuf, "\r\n");
        while (line)
        {
            sendLen += sprintf(sendBuf + sendLen, "%s\r\n", line);
            line     = strtok(NULL, "\r\n");
        }

        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendto(sockfd, sendBuf, strlen(sendBuf), 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0)
        {
            perror("ERROR in sendto");
            goto L_ERROR;
        }

        /* Set timeout */
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;
        select(0, NULL, NULL, NULL, &timeout);
    }

L_ERROR:
    if (sockfd > 0)
    {
        close(sockfd);
    }

    return 0;
}
