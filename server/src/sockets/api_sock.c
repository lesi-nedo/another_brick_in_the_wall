#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include "../../includes/api_sock.h"
#include "../../includes/util.h"
#include "../../includes/conn.h"
#include "../../includes/files_s.h"


char *SOCK_NAME = NULL;
int sock_fd = -1;
int epoll_fd = 0;
struct epoll_event events[MAX_EVENTS];
size_t LOCK_ID = 0;

/**
 * @param SOCK_NAME: name of the socket to be open
 * @return: returns the file descriptor of the opened socket, -1 if error occured
 * @desc: initial socket
 */
int ini_sock(char *SOCK_NAME){
    int lis_fd = 0;
    int res = 0;

    SYSCALL_RETURN(socket, lis_fd, socket(AF_UNIX, SOCK_STREAM, 0), -1, "Bye, I'll miss you part 1. <3.\n", NULL);
    struct sockaddr_un my_server;

    memset(&my_server, 0, sizeof(my_server));
    my_server.sun_family = AF_UNIX;
    strncpy(my_server.sun_path, SOCK_NAME, strlen(SOCK_NAME)+1);
    SYSCALL_RETURN(bind, res,  bind(lis_fd, (struct sockaddr*)&my_server, sizeof(my_server)), -1, "I need some time off, no hard feelings.\n", NULL);
    SYSCALL_RETURN(listen, res,  listen(lis_fd, MAX_LOG), -1, "Nel mezzo del cammin di tua vita hai incotrato un errore.\n", NULL);
    return lis_fd;
}
/**
 * @param SOCK_NAME: name of the socket to be open
 * @return: returns the file descriptor of the opened socket, -1 if error occurred
 * @desc: initial socket client side
 */
int ini_sock_client(const char *SOCK_NAME){
    int lis_fd = 0;
    int res = 0;

    SYSCALL_RETURN(socket, lis_fd, socket(AF_UNIX, SOCK_STREAM, 0), -1, "Bye, I'll miss you part 2. <3.\n", NULL);
    struct sockaddr_un my_server;

    memset(&my_server, 0, sizeof(my_server));
    my_server.sun_family = AF_UNIX;
    strncpy(my_server.sun_path, SOCK_NAME, strlen(SOCK_NAME)+1);
    errno =0;
    SYSCALL_RETURN(connect, res,  connect(lis_fd, (struct sockaddr*)&my_server, sizeof(my_server)), -1, "I'll try to connect again, Boss...\n", NULL);
    return lis_fd;
}
/**
 * @param lis_fd: file descriptor to set as O_NONBLOCK
 * @return: 0 for success, -1 otherwise
 */
int non_blocking(int lis_fd){
    int flags = 0;
    SYSCALL_RETURN(fcntl, flags, fcntl(lis_fd, F_GETFL, 0), -1, "May the force be with you.\n", NULL);
    SYSCALL_RETURN(fcntl, flags, fcntl(lis_fd, F_SETFL, flags| O_NONBLOCK), -1, "It was really quick, but it was really intense.\n", NULL);
    return 0;
}

/**
 * @param ep_fd: file descriptor created with epoll_create
 * @param fd: file descriptor to be deleted from listening
 * @param state: events state
 */
int delete_event(int ep_fd, int fd, int state){
    struct epoll_event event;
    event.events = state;
    event.data.fd = fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_DEL, fd, &event) < 0)
        return -1;
    return 0;
}

/**
 * @param ep_fd: file descriptor created with epoll_create
 * @param fd: file descriptor to be add to listening list
 * @param state: events state [EPOLLIN, ...]
 */
int add_event(int ep_fd, int fd, int state){
    struct epoll_event event;
    event.events = state;
    event.data.fd = fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &event) < 0)
        return -1;
    return 0;
}
/**
 * @param ep_fd: file descriptor created with epoll_create
 * @param fd: file descriptor to be modify
 * @param state: events state [EPOLLIN, ...]
 */

int modify_event(int ep_fd, int fd, int state){
    struct epoll_event event;
    event.events = state;
    event.data.fd = fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &event) < 0)
        return -1;
    return 0;
}
/**
 * @param ep_fd: file descriptor created with epoll_create
 * @param sock_fd: socket file descriptor to get data from
 * @param buff: where to save data
 * @param size: the size of data to be read
 */
int read_from(int ep_fd ,int sock_fd, void *buff, int size){
    int n = 0;
    while((n= readn(sock_fd, buff, size))==-1){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            print_error("Here something stranger has happened, I'll let you figure it out. t.v.b\n errno:%s\n", strerror(errno));
            delete_event(ep_fd, sock_fd, EPOLLIN);
            close(sock_fd);
            return -1;
        }
    }
    if(n==0){
        print_error("Hun, server was closed, are you sad now?\nerrno: %s\n", strerror(errno));
        delete_event(ep_fd, sock_fd, EPOLLIN);
        return 0;
    }
    return 1;

}

/**
 * @param ep_fd: file descriptor created with epoll_create
 * @param sock_fd: socket file descriptor to push data to
 * @param buff: from where get data
 * @param size: the size of data to be transferred
 */
int write_to(int ep_fd, int sock_fd, void *buff, int len){
    int n =0;

    while((n =writen(sock_fd, buff, len, NULL)) == -1){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            print_error("Buddy, I was trying to read from socket and I failed. Do not be mad <3\nerrno: %s\n", strerror(errno));
            delete_event(ep_fd, sock_fd, EPOLLOUT);
            close(sock_fd);
            return -1;
        }
    }
    if(n == 0){
        print_error("Hun, server was closed, are you sad now (I failed in write_to)?\nerrno: %s\n", strerror(errno));
        delete_event(ep_fd, sock_fd, EPOLLOUT);
        return 0;
    }
    return 1;

}



int _ready_for(){
    int nready = 0;

    nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if(nready<0){
        print_error("Hey boy I hit a breaking point.\nerrno: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    for(size_t i=0; i<nready; i++){
        if(events[i].events & EPOLLIN){
            return EPOLLIN;
        } else return EPOLLOUT;
    }
    return -1;
}

int _helper_send(const char *pathname, size_t *ret){
    size_t to_wrt = 0;
    int res = 0;

    assert(_ready_for() & EPOLLOUT);
    to_wrt = strlen(pathname)+1;
    res = write_to(epoll_fd, sock_fd, &to_wrt, sizeof(to_wrt));
    assert(_ready_for() & EPOLLOUT);
    res = write_to(epoll_fd, sock_fd, (void *)pathname, to_wrt);
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    to_wrt = 1;
    res= read_from(epoll_fd, sock_fd, &to_wrt, sizeof(to_wrt));
    if(res == -1) return -1;
    if(to_wrt == 0){
        errno = EINVAL;
        print_error("Pass some better arguments, Boss.\nopenFile: %s\n", strerror(errno));
        return -1;
    }
    *ret = to_wrt;
    return 0;
}

int openConnection(const char *sockname, int msec, const struct timespec abstime){
    if(!sockname){
        errno = EINVAL;
        return -1;
    }
    unsigned long  wait = (abstime.tv_sec*1e+9)+abstime.tv_nsec;
    unsigned long total = 0;
    int res = ini_sock_client(sockname);
    while(res == -1 && total < wait){
        msleep(msec);
        total += msec*1e+6;
        res = ini_sock_client(sockname);
    }
    if(res==-1){
        errno = ECONNREFUSED;
        return -1;
    } else {
        sock_fd = res;
    }
    if((epoll_fd = epoll_create1(0))< 0){
        print_error("I give up.\nerrno: %s\n", strerror(errno));
        return -1;
    }
    return add_event(epoll_fd, sock_fd, EPOLLOUT);
}

int closeConnection(const char *sockname){
    if(strcmp(sockname, SOCK_NAME) != 0){
        errno =EINVAL;
        return -1;
    }
    if(sock_fd == -1){
        errno = EBADFD;
        return -1;
    }
    close(sock_fd);
    return 0;
}

int openFile(const char *pathname, int flags){
    assert(sock_fd > 0);
    if(pathname == NULL){
        errno = EINVAL;
        return -1;
    }
    FILE *fl = fopen("my_id.txt", "w");
    if(fl == NULL){
        print_error("With tears in the eyes I have to announce I failed to open.\nerrno: %s\n", strerror(errno));
        return -1;
    }
    int nready = 0;
    size_t to_wrt_ID[2];
    to_wrt_ID[0] = 0;
    to_wrt_ID[1] = LOCK_ID;
    int res =0;

        nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(nready<0){
            print_error("Hey boy I hit a breaking point.\nerrno: %s", strerror(errno));
            return -1;
        }
    assert(_ready_for() & EPOLLOUT);
    switch (flags) {
        case O_CREATE_M:
            to_wrt_ID[0] = OPEN_F_C;
            res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
            break;
        case O_LOCK_M:
            to_wrt_ID[0] = OPEN_F_L;
            res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
            break;
        case (O_CREATE_M | O_LOCK_M):
            to_wrt_ID[0] = OPEN_F_L_C;
            res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    if(res == -1){
        return -1;
    }
    res=_helper_send(pathname, &LOCK_ID);
    if(res == -1) return -1;
    fprintf(fl, "%zd", LOCK_ID);
    fclose(fl);
    return 0;
}
