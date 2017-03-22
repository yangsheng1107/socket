#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#define BACKLOG 10
#define MAX_EVENTS BACKLOG
#define POLL_EXPIRE      (0)


static int sockfd = 0;
static int  epollfd = 0;

typedef struct POLL_INFO {
    struct pollfd events[MAX_EVENTS];
    int numfd;
}POLL_INFO_st;

static POLL_INFO_st info;

/*
* =================================================
*
*           Basic API
*
* =================================================
*/
/***
 * setnonblocking :
 * set socket to non blocking mode
 */
void setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock,F_GETFL);
    if(opts<0) {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0) {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }  
}

/*
* =================================================
*
*           epoll API
*
* =================================================
*/
/***
 * poll_create:
 * add socket description to event
 */
int poll_create(POLL_INFO_st * ptr)
{
    int i = 0;

    for ( i = 0; i < MAX_EVENTS; i++) {
        ptr->events[i].fd = -1;
        ptr->events[i].events = 0;
        ptr->events[i].revents = 0;
    }

    return 0;
}

/***
 * poll_add :
 * add socket description to event
 */
int poll_add(POLL_INFO_st * ptr, int sockfd, int events)
{
    int ret = -1, i = 0;

    for ( i = 0; i < MAX_EVENTS; i++) {
        if (ptr->events[i].fd == -1)
        {
            ptr->events[i].fd = sockfd;
            ptr->events[i].events =  POLLIN | POLLHUP ;
            ptr->numfd++;
            ret = 0;
            break;
        }
    }

    return ret;
}

/***
 * poll_del :
 * add socket description to event
 */
int poll_del(POLL_INFO_st * ptr, int sockfd)
{
    int i = 0;

    for ( i = 0; i < MAX_EVENTS; i++) {
        if (ptr->events[i].fd == sockfd)
        {
            ptr->events[i].fd = -1;
            ptr->events[i].events = 0;
            ptr->numfd--;
            break;
        }
    }

    return 0;
}

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
    if (epollfd > 0) {
        close(epollfd);
        epollfd = 0;
    }

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
/*
* =================================================
*
*           Socket API
*
* =================================================
*/
/***
 * do_recv :
 * socket recv
 */
int do_recv(int sockfd)
{
    int recvlen;
    char recvBuf[1024] = {0x00};
    recvlen = recv(sockfd, recvBuf, sizeof(recvBuf), 0);
    if (recvlen <= 0) {
        perror("recv");
        return -1;
    }
    printf("Receive(%d): len=[%d]\r\n", sockfd, recvlen);
    printf("%s\n", recvBuf);

    return recvlen;
}

/***
 * do_send :
 * send 
 */
int do_send(int sockfd)
{
    printf("(%s)(%d) socket = %d\r\n", __func__,__LINE__, sockfd);
    return 0;
}

/***
 * do_accept :
 * accept 
 */
int do_accept(POLL_INFO_st * ptr, int sockfd)
{
    int conn_sock;
    struct sockaddr_un remote_addr;
    size_t sin_size = sizeof(struct sockaddr_un);

    conn_sock = accept(sockfd,
            (struct sockaddr *) &remote_addr,(socklen_t *) &sin_size);
    if (conn_sock == -1) {
        perror("accept");
        return -1;
    }
    setnonblocking(conn_sock);

    if (poll_add(ptr, conn_sock, (POLLIN |POLLPRI| POLLOUT | POLLHUP)) < 0) {
        perror("poll_add");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_un server_addr;
    int nfds, milliseconds = 1000 ;
    int n = 0, len = 0;
    char *hostname;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <sock file>\n", argv[0]);
        goto L_ERROR;
    }

    hostname = argv[1];

    if (signal_setup() < 0) {
        perror("signalSetup");
        goto L_ERROR;
    }

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, hostname);
    unlink(server_addr.sun_path);

    if ((sockfd = socket(AF_UNIX,  SOCK_STREAM,  0)) == -1) {
        perror("socket");
        goto L_ERROR;
    }

    len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family);
    if (bind(sockfd,  (struct sockaddr *)&server_addr, len) == -1) {
        perror("bind");
        goto L_ERROR;
    }

    if (listen(sockfd,  BACKLOG) == -1) {
        perror("listen");
        goto L_ERROR;
    }

    if (poll_create(&info) < 0) {
        perror("poll_add");
        goto L_ERROR;
    }

    if (poll_add(&info, sockfd, (POLLIN |POLLPRI| POLLOUT | POLLHUP)) < 0) {
        perror("poll_add");
        goto L_ERROR;
    }

    for (;;) {
        nfds = poll(info.events, MAX_EVENTS, milliseconds);
        switch( nfds )
        {
            case POLL_EXPIRE:
                //printf("Timeout has expired !\n");
                break;
            case POLL_ERR:
                perror("poll");
                break;
            default:
                for (n = 0; n < MAX_EVENTS; n++) {
                    //printf("info.events[%d].fd = %d!!\n", n, info.events[n].fd);
                    //printf("info.events[%d].revents = %d!!\n", n, info.events[n].revents);

                    if (info.events[n].revents & POLLIN) {
                        if (n == 0) {
                            do_accept(&info, info.events[n].fd);
                        } else {
                            int nread = 0;
                            ioctl(info.events[n].fd, FIONREAD, &nread);
                            if( nread > 0 ) {
                                do_recv(info.events[n].fd);
                            } else {
                                close(info.events[n].fd);
                            }
                        }
                    }

                    if (info.events[n].revents & POLLOUT) {
                        do_send(info.events[n].fd);
                    }

                    if (info.events[n].revents & (POLLERR|POLLHUP|POLLNVAL)) {
                        printf("(%d)delete socket %d \n", __LINE__, info.events[n].fd) ;
                        poll_del(&info, info.events[n].fd);
                    }
                }
                break;
        }
    }

L_ERROR:

    if (sockfd > 0) {
        close(sockfd);
        sockfd = 0;
    }

    return 0;
}
