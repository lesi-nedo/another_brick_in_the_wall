#include <assert.h>
#include <libgen.h>
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
    memset(&event, 0, sizeof(struct epoll_event));
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
    memset(&event, 0, sizeof(struct epoll_event));
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
    memset(&event, 0, sizeof(struct epoll_event));
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
 * @return 1 on success, 0 closed server, -1 error
 */
int read_from(int ep_fd ,int sock_fd, void *buff, int size){
    int n = 0;
    n= readn(sock_fd, buff, size, NULL);
    if(n < -1){
        print_error("Hun, an error occured \nerrno: %s\n", strerror(errno));
        delete_event(ep_fd, sock_fd, EPOLLIN);
        return -1;
    }
    if(n==0){
        print_error("Hun, server was closed, are you sad now?\nerrno: %s\n", strerror(errno));
        delete_event(ep_fd, sock_fd, EPOLLIN);
        return -1;
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

    n =writen(sock_fd, buff, len, NULL);
    if(n < 0){
        print_error("Buddy, I was trying to read from socket and I failed. Do not be mad <3\nerrno: %s\n", strerror(errno));
        delete_event(ep_fd, sock_fd, EPOLLOUT);
        close(sock_fd);
        return -1;
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
    //resp[0] if 1 then it went something wrong and in resp[1] will be save errno value 
    size_t resp[2];
    resp[0]=0;
    resp[1]=0;
    int res = 0;

    to_wrt = strlen(pathname)+1;
    res = write_to(epoll_fd, sock_fd, &to_wrt, sizeof(to_wrt));
    res = write_to(epoll_fd, sock_fd, (void *)pathname, to_wrt);
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    to_wrt = 1;
    res= read_from(epoll_fd, sock_fd, resp, sizeof(resp));
    if(res == -1) return -1;
    if(resp[0] != IS_NOT_ERROR){
        print_error("Pass some better arguments, Boss.\nError: %s\n", strerror(resp[1]));
        return -1;
    }
    *ret = resp[1];
    return 0;
}

int _helper_send_data(void *data, size_t len){
    int res =0;
    size_t resp[2] = {0};
    res = write_to(epoll_fd, sock_fd, &len, sizeof(len));
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    res = read_from(epoll_fd, sock_fd, resp, sizeof(resp));
    if(res == -1) return -1;
    if( resp[0]!= IS_NOT_ERROR){
        errno =resp[1];
        return -1;
    }
    modify_event(epoll_fd, sock_fd, EPOLLOUT);
    assert(_ready_for() & EPOLLOUT);
    res = write_to(epoll_fd, sock_fd, data, len);
    return res;
}

int helper_receive(void **buff, size_t *size){
    int res = 0;
    size_t resp[2];
    resp[0]=0;
    resp[1]=0;
    res = read_from(epoll_fd, sock_fd, resp, sizeof(resp));
    if(res < 1) return res;
    if(resp[0] != IS_NOT_ERROR){
        errno =resp[1];
        return -1;
    }
    *size=resp[1];
    assert(*size > 0);
    *buff=(void *)calloc(*size, sizeof(u_int8_t));
    if(buff  == NULL){
        return -1;
    }
    res =read_from(epoll_fd, sock_fd, *buff, *size);
    return res;
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
    size_t to_wrt_ID[2];
    to_wrt_ID[0] = 0;
    to_wrt_ID[1] = LOCK_ID;
    int res =0;
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
    return res;
}
/**
 * This function was taken from esercitazione n.10
 */
char* cwd() {
  char* buf = malloc(NAME_MAX*sizeof(char));
  if (!buf) {
    perror("cwd malloc");
    return NULL;
  }
  if (getcwd(buf, NAME_MAX) == NULL) {
    if (errno==ERANGE) {
      char* buf2 = realloc(buf, 2*NAME_MAX*sizeof(char));
      if (!buf2) {
	perror("cwd realloc");
	free(buf);
	return NULL;
      }
      buf = buf2;
      if (getcwd(buf,2*NAME_MAX)==NULL) { 
	free(buf);
	return NULL;
      }
    } else {
      free(buf);
      return NULL;
    }
  }
  return buf;
}


int readFile(const char*pathname, void **buff, size_t *size){
    if(pathname==NULL || buff==NULL || size==NULL){
        errno = EINVAL;
        return -1;
    }
    size_t to_wrt_ID[2];
    int res = 0;
    to_wrt_ID[0] = READ_F;
    to_wrt_ID[1] = LOCK_ID;
    modify_event(epoll_fd, sock_fd, EPOLLOUT);
    assert(_ready_for() & EPOLLOUT);
    res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
    if(res == -1){
        return -1;
    }
    res=_helper_send(pathname, &LOCK_ID);
    if(res == -1) return -1;
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    res = helper_receive(buff, size);
    if(res == 1){
        return 0;
    } else if(res == 0){
        errno = EOWNERDEAD;
        return -1;
    }
    return -1;
}

int readNFiles(int N, const char *dirname){
    if(dirname==NULL){
        errno = EINVAL;
        return -1;
    }
    char *curr_dir = cwd();
    char *file_name = NULL;
    void *buff = NULL;
    size_t size =0;
    size_t to_wrt_N = 0;
    if(N <= 0){
        to_wrt_N = 0;
    }
    size_t to_wrt_ID[2];
    int res = 0;
    to_wrt_ID[0] = READ_NF;
    to_wrt_ID[1] = LOCK_ID;
    modify_event(epoll_fd, sock_fd, EPOLLOUT);
    res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
    if(res == -1){
        free(curr_dir);
        return -1;
    }
    if(chdir(dirname) ==-1){
        free(curr_dir);
        return -1;
    }
    res = write_to(epoll_fd, sock_fd, &to_wrt_N, sizeof(to_wrt_N));
    if(res == -1){
        free(curr_dir);
        return -1;
    }
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    while(helper_receive((void**)&file_name, &size) > 0){
        char *name = basename(file_name);
        FILE *f = fopen(name, "w");
        if(f == NULL){
            free(curr_dir);
            free(file_name);
            return -1;
        }
        res = helper_receive(&buff, &size);
        if(res < 1){
            if(buff) free(buff);
            free(curr_dir);
            free(file_name);
            fclose(f);
            return -1;
        }
        fprintf(f, "%s", (char*)buff);
        fclose(f);
        free(buff);
        free(file_name);
        buff =NULL;
        file_name=NULL;
    }
    if(file_name) free(file_name);
    if(chdir(curr_dir)==-1){
        free(curr_dir);
    }
    free(curr_dir);
    if(errno != 0) return -1;
    return 0;
}

int _helper_victim_save(const char *dirname){
    char *name_victim =NULL;
    int res =0;
    size_t size_victim = 0;
    void *data_victim = NULL;
    char *curr_dir = cwd();
    if(curr_dir == NULL){
            return -1;
    }
    if(chdir(dirname)==-1){
        free(curr_dir);
        return -1;
    }
   while((res= helper_receive((void**)&name_victim, &size_victim)) != -1){
        res =helper_receive(&data_victim, &size_victim);
        if(res == -1){
            if(data_victim) free(data_victim);
            free(name_victim);
            free(curr_dir);
            return -1;
        }
        char *name = basename(name_victim);
        FILE *fp= fopen(name,"w");
        if(fp == NULL){
            free(name_victim);
            free(data_victim);
            free(curr_dir);
            return -1;
        }
        res = fwrite(data_victim,sizeof(u_int8_t), size_victim, fp);
        if(res != size_victim){
            free(name_victim);
            free(data_victim);
            free(curr_dir);
            fclose(fp);
            return -1;
        }
        
        free(name_victim);
        free(data_victim);
        fclose(fp);
   }
   if(chdir(curr_dir)==-1){
        free(curr_dir);
        return -1;
    }
    if(res == -1 && errno > 0){
        free(curr_dir);
        if(name_victim) free(name_victim);
        return -1;
    }
    free(curr_dir);
    return 0;
}

int _helper_victim(){
    char *name_victim =NULL;
    int res =0;
    size_t size_victim = 0;
    void *data_victim = NULL;
    while((res= helper_receive((void**)&name_victim, &size_victim)) != -1){
    res =helper_receive(&data_victim, &size_victim);
    if(res == -1){
        if(data_victim) free(data_victim);
        free(name_victim);
        return -1;
    }
    free(name_victim);
    free(data_victim);
    }
    if(res == -1 && errno > 0){ 
        if(name_victim) free(name_victim);
        return -1;
    }
    return 0;
}

int appendToFile(const char* pathname, void *data, size_t size, const char* dirname){
    if(pathname == NULL || data == NULL || size <= 0){
        print_error("Pass some better arguments to appendFile, bro.\n", NULL);
        errno =EINVAL;
        return -1;
    }
    int res = 0;
    size_t to_wrt_ID[2];
    to_wrt_ID[0] = WRITE_F;
    to_wrt_ID[1] = LOCK_ID;
    res = modify_event(epoll_fd, sock_fd, EPOLLOUT);
    if(res == -1){
        return -1;
    }
    assert(_ready_for() & EPOLLOUT);
    res = write_to(epoll_fd, sock_fd, &to_wrt_ID, sizeof(to_wrt_ID));
    if(res == -1){
        return -1;
    }
    size_t my_id = 0;
    res = _helper_send(pathname, &my_id);
    if(res == -1){
        return -1;
    }
    #ifdef DEBUG
    assert(LOCK_ID == my_id);
    #endif // DEBUG
    res = modify_event(epoll_fd, sock_fd, EPOLLOUT);
    if(res == -1){
        return -1;
    }
    assert(_ready_for() & EPOLLOUT);
    res = _helper_send_data(data, size);
    if(res == -1) return -1;
    modify_event(epoll_fd, sock_fd, EPOLLIN);
    assert(_ready_for() & EPOLLIN);
    if(dirname) res = _helper_victim_save(dirname);
    else res = _helper_victim();
    return res;
}

int writeFile(const char *pathname, const char *dirname){
    if(pathname == NULL){
        errno =EINVAL;
        return -1;
    }
    long size = 0;
    int res = 0;
    void *data = NULL;
    FILE *fp = fopen(pathname, "r");
    if(fp == NULL) return -1;
    res = fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    if(size == -1) return -1;
    if(size == 0){
        errno = EINVAL;
        return -1;
    }
    res = fseek(fp, 0, SEEK_SET);
    data = calloc(size, sizeof(u_int8_t));
    if(data == NULL || res == -1){
        return -1;
    }
    res = fread(data, 1, size, fp);
    fclose(fp);
    if(res != size){
        free(data);
        return -1;
    }
    res = modify_event(epoll_fd, sock_fd, EPOLLOUT);
    if(res == -1){
        free(data);
        return -1;
    }
    res = openFile(pathname, O_CREATE_M | O_LOCK_M);
    if(res == -1){
        free(data);
        return -1;
    }
    res = modify_event(epoll_fd, sock_fd, EPOLLIN);
    if(res == -1){
        free(data);
        return -1;
    }
    assert(_ready_for() & EPOLLIN);
    if(dirname) res = _helper_victim_save(dirname);
    else res = _helper_victim();
    if(res == -1){ 
        free(data);
        return -1;
    }
    res= appendToFile(pathname, data, size, dirname);
    free(data);
    return res;
}

char *get_full_path(char *file, size_t size){
    char *curr_dir = cwd();
    if(curr_dir == NULL){
        return NULL;
    }
    size_t null_at = 0;
    char* buf2 = realloc(curr_dir, (NAME_MAX+size+1)*sizeof(char));
    if(buf2 == NULL){
        print_error("Boss I failed with error: %s \n", strerror(errno));
        free(curr_dir);
        return NULL;
    }
    curr_dir = buf2;
    null_at =strcspn(curr_dir, "\0");
    curr_dir[null_at]='/';
    curr_dir[null_at+1]='\0';
    strncat(curr_dir, file, size);
    return curr_dir;
}