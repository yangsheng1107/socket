/*
 * client.c - A simple TCP client
 * usage: client <sock file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


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
* =================================================
*
*           Signal API
*
* =================================================
*/
/***
 * signal_handler :
 * handle signal
 */
void signal_handler(int signum, siginfo_t *siginfo, void *context)
{
    pid_t sender_pid  = siginfo->si_pid;

    printf("(%s)(%d) catch signal = %d from %d\r\n", __func__,__LINE__, signum, sender_pid);

    if (sockfd > 0) {
        close(sockfd);
        sockfd = 0;
    }

    exit(signum);
}

/***
 * signal_setup :
 * setup signal
 */
int signal_setup()
{
    struct sigaction sa_usr;
    int signTable[] = { SIGINT, SIGQUIT, SIGSEGV, SIGTERM, -1 };  //2,3,11,15
    int i           = 0;

    // prepare sigaction
    memset(&sa_usr, 0x00, sizeof(sa_usr));
    sa_usr.sa_sigaction = signal_handler;
    sa_usr.sa_flags     = SA_SIGINFO; // get detail info

    while (signTable[i] != -1) {
        if (sigaction(signTable[i], &sa_usr, NULL) < 0) {
            char errMsg[32] = { 0x00 };
            snprintf(errMsg, sizeof(errMsg) - 1, "sigaction : %d", signTable[i]);
            perror(errMsg);
            return -1;
        }
        i++;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int   sendLen, len = 0;
    struct sockaddr_un serveraddr;
    char *hostname, *line = NULL;
    char  sendBuf[BUFSIZE], sourceBuf[BUFSIZE];
    struct timeval timeout;

    if (signal_setup() < 0) {
        perror("signalSetup");
        goto L_ERROR;
    }

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <sock file>\n", argv[0]);
        goto L_ERROR;
    }

    hostname = argv[1];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        goto L_ERROR;
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, hostname);
    len = strlen(serveraddr.sun_path) + sizeof(serveraddr.sun_family);

    if (connect(sockfd, (struct sockaddr *)&serveraddr, len) < 0)
    {
        perror("ERROR connecting");
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
            perror("ERROR writing to socket");
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
