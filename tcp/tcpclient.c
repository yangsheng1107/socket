/*
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

int sockfd = 0;
/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(0);
}


static void close_sigint(int dummy)
{
    printf("[%d] dummy = %d\n", __LINE__, dummy);

    /* close client connection */
    if (sockfd > 0)
    {
        close(sockfd);

    }
    exit(0);
}

int main(int argc, char **argv)
{
    int   portno, sendLen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, *line = NULL;
    char  sendBuf[BUFSIZE], sourceBuf[BUFSIZE];
    struct timeval timeout;

    signal(SIGINT, close_sigint);  //02
    signal(SIGQUIT, close_sigint); //03
    signal(SIGKILL, close_sigint); //09
    signal(SIGSEGV, close_sigint); //11
    signal(SIGTERM, close_sigint); //15

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        goto L_ERROR;
    }

    hostname = argv[1];
    portno   = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR opening socket");
        goto L_ERROR;
    }

    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        goto L_ERROR;
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        error("ERROR connecting");
        goto L_ERROR;
    }

    /* get message line from the user */
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

        sendLen = write(sockfd, sendBuf, strlen(sendBuf));
        if (sendLen < 0)
        {
            error("ERROR writing to socket");
            break;
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
