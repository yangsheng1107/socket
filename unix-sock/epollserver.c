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
#include <sys/epoll.h>

#define BACKLOG 10
#define MAX_EVENTS BACKLOG
#define POLL_EXPIRE      (0)

static int sockfd = 0, epollfd = 0;

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
 * event_add :
 * add socket description to event
 */
int event_add(int epollfd, int sockfd, int events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        return -1;
    }

    return 0;
}

/***
 * event_mod :
 * modify socket description to event
 */
int event_mod(int epollfd, int sockfd, int events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1) {
        return -1;
    }

    return 0;
}

/***
 * event_del :
 * delete socket description to event
 */
int event_del(int epollfd, int sockfd)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL) == -1) {
        return -1;
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

    return 0;
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
int do_accept(int epollfd, int sockfd)
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

    /* epoll will wake up for the following events :
    * EPOLLIN : The associated file is available for read(2) operations.
    * EPOLLOUT : The associated file is available for write(2) operations.
    * EPOLLRDHUP : Stream socket peer closed connection, or shut down writing 
    *              half of connection. (This flag is especially useful for writing simple 
    *              code to detect peer shutdown when using Edge Triggered monitoring.)
    * EPOLLERR : Error condition happened on the associated file descriptor. 
    * epoll_wait(2) will always wait for this event; it is not necessary to set it in events.
    */
    if (event_add(epollfd, conn_sock, (EPOLLOUT | EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET) ) < 0) {
        perror("epoll_ctl: conn_sock");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_un server_addr;
    struct epoll_event events[MAX_EVENTS];
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

    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd == -1) {
        perror("epoll_create");
        goto L_ERROR;
    }
    printf("epoll_create\n");
    /* epoll will wake up for the following events :
    * EPOLLIN : The associated file is available for read(2) operations.
    * EPOLLOUT : The associated file is available for write(2) operations.
    * EPOLLRDHUP : Stream socket peer closed connection, or shut down writing 
    *              half of connection. (This flag is especially useful for writing simple 
    *              code to detect peer shutdown when using Edge Triggered monitoring.)
    * EPOLLERR : Error condition happened on the associated file descriptor. 
    * epoll_wait(2) will always wait for this event; it is not necessary to set it in events.
    */
    if (event_add(epollfd, sockfd, (EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET)) < 0) {
        perror("epoll_ctl: sockfd");
        goto L_ERROR;
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, milliseconds);
        switch( nfds )
        {
            case POLL_EXPIRE:
                //printf("Timeout has expired !\n");
                break;
            case POLL_ERR:
                perror("poll_pwait");
                break;
            default:
                for (n = 0; n < nfds; ++n) {
                    if (events[n].events & EPOLLIN) {
                        if (events[n].data.fd == sockfd) {
                            do_accept(epollfd, sockfd);
                        } else {
                            do_recv(events[n].data.fd);
                        }
                    }

                    if (events[n].events & EPOLLOUT) {
                        do_send(events[n].data.fd);
                    }

                    if (events[n].events & (EPOLLRDHUP | EPOLLHUP)) {
                        event_del(epollfd, events[n].data.fd);
                        close(events[n].data.fd);
                        printf("delete socket %d \n",events[n].data.fd) ;
                        events[n].data.fd = 0;
                    }

                    if (events[n].events & EPOLLERR) {
                        perror("epoll");
                        continue;
                    }
                }
                break;
        }
    }

L_ERROR:

    if (epollfd > 0) {
        close(epollfd);
        epollfd = 0;
    }

    if (sockfd > 0) {
        close(sockfd);
        sockfd = 0;
    }

    return 0;
}