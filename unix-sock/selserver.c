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

#define BUF_SIZE   4096

#define BACKLOG 10
#define MAX_EVENTS BACKLOG
#define SELE_EXPIRE      (0)
#define SELE_ERR      (-1)

fd_set read_max_fds;
int fdmax = 0, sockfd = -1;

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
*           select API
*
* =================================================
*/
/***
 * fd_create :
 * add socket description to event
 */
int fd_create()
{
    FD_ZERO(&read_max_fds);
    return 0;
}

/***
 * fd_add :
 * add socket description to event
 */
int fd_add(int sockfd)
{
    FD_SET(sockfd, &read_max_fds);
    if (sockfd > fdmax) {
        fdmax = sockfd;
    }

    return 0;
}

/***
 * fd_del :
 * add socket description to event
 */
int fd_del(int sockfd)
{
    FD_CLR(sockfd, &read_max_fds);
    return 0;
}

/***
 * fd_get :
 * add socket description to event
 */
fd_set fd_get()
{
    return read_max_fds;
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
int do_accept(int sockfd)
{
    int conn_sock;
    struct sockaddr_un  remote_addr;
    size_t sin_size = sizeof(struct sockaddr_un);

    conn_sock = accept(sockfd,
            (struct sockaddr *) &remote_addr,(socklen_t *) &sin_size);
    if (conn_sock == -1) {
        perror("accept");
        return -1;
    }

    setnonblocking(conn_sock);

    fd_add(conn_sock);

    return 0;
}

int main(int argc, char **argv)
{
    int                ret, n = 0, flag = 1, len = 0;
    fd_set             rfds;
    struct timeval     timeout;
    struct sockaddr_un server_addr;
    char              *hostname;

    if (argc != 2)
    {
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

    /* Create endpoint */
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
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

    len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family);
    if (bind(sockfd, (struct sockaddr *)&server_addr, len) < 0)
    {
        perror("bind()");
        goto L_ERROR;
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen()");
        goto L_ERROR;
    }

    fd_create();
    fd_add(sockfd);

    for (;;) {
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        rfds = fd_get();
        ret  = select(fdmax + 1, &rfds, NULL, NULL, &timeout);

        switch( ret )
        {
            case SELE_EXPIRE:
                //printf("Timeout has expired !\n");
                break;
            case SELE_ERR:
                perror("select");
                break;
            default:
                for (n = 0; n < BACKLOG; n++) {
                    if (FD_ISSET(n, &rfds)) { 
                        if (n == sockfd) {
                            do_accept(n);
                        } else {
                            int nread = 0;
                            ioctl(n, FIONREAD, &nread);
                            if( nread > 0 ) {
                                do_recv(n);
                            } else {
                                close(n);
                                fd_del(n);
                            }
                        }
                    }
                }
                break;
        }
    }

L_ERROR:
    if (sockfd > 0)
    {
        close(sockfd);
    }

    return 0;
}
