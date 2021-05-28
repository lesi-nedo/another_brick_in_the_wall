#ifndef API_SOCK_H
#define API_SOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/epoll.h>


#define MAX_LOG 64
#define MAX_FDS 16*1024
#define SENDBUFF_SIZE 1024
#define RECEIVEBUFF_SIZE 1024
#define OPEN_F_L 0
#define OPEN_F_C 1
#define OPEN_F_L_C 2
#define READ_F 3
#define READ_NF 4
#define WRITE_F 5
#define APPEND_F 6
#define LOCK_F 7
#define UNLOCK_F 8
#define COSE_F 9
#define REMOVE_F 10
#define CLOS_CON 11
#define NUM_AP 12
#define NOT_OK_M 0
#define MAX_EVENTS 64
#define O_CREATE_M 1
#define O_LOCK_M 2

extern char *SOCK_NAME;
extern int sock_fd;
extern int epoll_fd;
extern struct epoll_event events[MAX_EVENTS];
extern size_t LOCK_ID;

typedef struct {
    int state;
    size_t response;
    void *sendbuff;
    int sendbuff_end;
    int sendptr;
} conn_state_t;


extern conn_state_t global_state[MAX_FDS];

int ini_sock(char *);
int non_blocking(int lis_fd);
int ini_sock_client(const char *);
int delete_event(int , int, int);
int add_event(int, int, int);
int read_from(int,int, void *, int);
int write_to(int ep_fd, int sock_fd, void *buff, int len);
int modify_event(int, int, int);
int openConnection(const char *, int, const struct timespec);
int openFile(const char *, int);
int closeConnection(const char *);

#endif // !API_SOCK_H