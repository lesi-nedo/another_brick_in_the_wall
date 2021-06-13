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
signed long long int LOCK_ID = 0;
long SIZE_FILE =0;


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
        delete_event(ep_fd, sock_fd, EPOLLIN);
        return -1;
    }
    if(n==0){
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
    errno =0;
    n =writen(sock_fd, buff, len, NULL);
    if(n < 0){
        delete_event(ep_fd, sock_fd, EPOLLOUT);
        close(sock_fd);
        return -1;
    }
    if(n == 0){
        delete_event(ep_fd, sock_fd, EPOLLOUT);
        return 0;
    }
    return 1;

}

/**
 * @brief:Waits for data from server
 */
int _ready_for(){
    int nready = 0;
    nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if(nready<0){
        exit(EXIT_FAILURE);
    }
    for(size_t i=0; i<nready; i++){
        if(events[i].events & EPOLLIN){
            return EPOLLIN;
        } else return EPOLLOUT;
    }
    return -1;
}

/**
 * @brief: function that helps the main API to send pathname and pathname size's to the server.
 * @param pathname: pathname to be sent to the server
 * @param size: size of pathname
 * @return 0 success, -1 fatal error.
 */

int _helper_send(const char *pathname, signed long long int *ret){
    signed long long int to_wrt = 0;
    //resp[0] if 1 then it went something wrong and in resp[1] will be save errno value
    signed long long int resp[2];
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
        errno=resp[1];
        return -1;
    }
    *ret = resp[1];
    return 0;
}

/**
 * @brief: helps API to send data to the server
 * @param data: the data to be send to the server
 * @param len: the length of the data
 * @return: 1 success, 0 server was closed, -1 fatal error has occured
 */
int _helper_send_data(void *data, signed long long int len){
    int res =0;
    signed long long int resp[2] = {0};
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

/**
 * @brief: gets the response from the server IMPORTANT if is used openFile with O_CREATE_M or O_OCREATE_M | O_LOCK_O than this function should be called after.
 * @param buff: buffer to store data.
 * @param size: size of data read from server.
 * @return 1 success, 0 closed server -1 error
 */
int helper_receive(void **buff, signed long long int *size){
    int res = 0;
    signed long long int resp[2];
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
    *buff=(void *)calloc(*size, sizeof(char));
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
    int lis_fd = 0;
    SYSCALL_RETURN(socket, lis_fd, socket(AF_UNIX, SOCK_STREAM, 0), -1, "Bye, I'll miss you part 2. <3.\n", NULL);
    struct sockaddr_un my_server;

    memset(&my_server, 0, sizeof(my_server));
    my_server.sun_family = AF_UNIX;
    strncpy(my_server.sun_path, SOCK_NAME, strlen(SOCK_NAME)+1);
    errno =0;
    int res =0;
    char path[] = {"../server/"};
    char *curr_dir =cwd();
    if(curr_dir == NULL) return -1;
    res = chdir(path);
    if(res < 0){
        free(curr_dir);
        return -1;
    }
    unsigned long  wait = (abstime.tv_sec*1e+9)+abstime.tv_nsec;
    unsigned long total = msec == 0? wait: 0;
    res = connect(lis_fd, (struct sockaddr*)&my_server, sizeof(my_server));;
    while(res == -1 && total < wait){
        msleep(msec);
        total +=  msec*1e+6;
        res = connect(lis_fd, (struct sockaddr*)&my_server, sizeof(my_server));;
    }
    if(res==-1){
        errno = ECONNREFUSED;
        free(curr_dir);
        return -1;
    } else {
        sock_fd = lis_fd;
    }
    res = chdir(curr_dir);
    if(res < 0) return -1;
    free(curr_dir);
    if((epoll_fd = epoll_create1(0))< 0){
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
    signed long long int to_wrt_ID[2];
    to_wrt_ID[0] = 0;
    to_wrt_ID[1] = LOCK_ID;
    int res =0;
    modify_event(epoll_fd,sock_fd, EPOLLOUT);
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
    return _helper_send(pathname, &LOCK_ID);
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


int readFile(const char*pathname, void **buff, signed long long int *size){
    if(pathname==NULL || buff==NULL || size==NULL){
        errno = EINVAL;
        return -1;
    }
    signed long long int to_wrt_ID[2];
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
        res = read_from(epoll_fd, sock_fd, to_wrt_ID, sizeof(to_wrt_ID));
        if(res == -1) return -1;
        if(to_wrt_ID[0] != IS_NOT_ERROR){
            errno = to_wrt_ID[1];
            return -1;
        }
        return 0;
    } else if(res == 0){
        errno = EOWNERDEAD;
        return -1;
    }
    return -1;
}

int readNFiles(int N, const char *dirname){
    int yes=0;
    if(dirname==NULL){
        dirname="/dev/";
        yes=1;
    }
    char *curr_dir = cwd();
    if(curr_dir == NULL) return -1;
    char *file_name = NULL;
    void *buff = NULL;
    signed long long int size =0;
    signed long long int to_wrt_N = 0;
    if(N <= 0){
        to_wrt_N = 0;
    } else to_wrt_N = N;
    signed long long int to_wrt_ID[2];
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
    SIZE_FILE=0;
    while(helper_receive((void**)&file_name, &size) > 0){
        char *name = !yes? basename(file_name): "null";
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
        SIZE_FILE += size;
        fwrite(buff, sizeof(char), size, f);
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

/**
 * @brief: saves the victim in permanent memory
 * @param dirname: location where all victims will be saved
 * @return 0 success, -1 fatal error.
 */
int _helper_victim_save(const char *dirname){
    char *name_victim =NULL;
    int res =0;
    signed long long int size_victim = 0;
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
        res = fwrite(data_victim,sizeof(char), size_victim, fp);
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
/**
 * @brief: reads from socket the victim if presents the frees memory
 * @return: 0 success, -1 fatal error
 */
int _helper_victim(){
    char *name_victim =NULL;
    int res =0;
    signed long long int size_victim = 0;
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

int appendToFile(const char* pathname, void *data, signed long long int size, const char* dirname){
    if(pathname == NULL || data == NULL || size <= 0){
        errno =EINVAL;
        return -1;
    }
    int res = 0;
    signed long long int to_wrt_ID[2];
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
    signed long long int my_id = 0;
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
    signed long long int size = 0;
    int res = 0;
    void *data = NULL;
    FILE *fp = fopen(pathname, "r");
    if(fp == NULL) return -1;
    res = fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    SIZE_FILE = size;
    if(size == -1) return -1;
    if(size == 0){
        errno = EINVAL;
        return -1;
    }
    res = fseek(fp, 0, SEEK_SET);
    data = calloc(size, sizeof(char));
    if(data == NULL || res == -1){
        return -1;
    }
    res = fread(data, 1, size, fp);
    fclose(fp);
    if(res != size){
        free(data);
        return -1;
    }
    res = openFile(pathname, O_CREATE_M | O_LOCK_M);
    if(res == -1){
        free(data);
        return -1;
    }
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

/**
 * @brief: helps main API functions to remove/lock/unlock/close file
 * @param pathname: full path of the file to send to the server
 * @param op:  the index to be used in the array of function.
 */
int _helper_l_u_c_r(const char *pathname, int op){
    if(pathname == NULL){
        errno =EINVAL;
        return -1;
    }
    int res = 0;
    signed long long int to_wrt_ID[2];
    to_wrt_ID[0] = op;
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
    return _helper_send(pathname, &LOCK_ID);
}

int lockFile(const char *pathname){
    return _helper_l_u_c_r(pathname, LOCK_F);
}

int unlockFile(const char *pathname){
    return _helper_l_u_c_r(pathname, UNLOCK_F);
}

int closeFile(const char *pathname){
    return _helper_l_u_c_r(pathname, CLOSE_F);
}

int removeFile(const char *pathname){
    return _helper_l_u_c_r(pathname, REMOVE_F);
}

