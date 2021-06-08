#ifndef API_SOCK_H
#define API_SOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/epoll.h>

#define IS_NOT_ERROR 0
#define IS_ERROR 1
#define IS_EMPTY 2
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
#define LOCK_F 6
#define UNLOCK_F 7
#define CLOSE_F 8
#define REMOVE_F 9
#define NUM_AP 10
#define NOT_OK_M 0
#define MAX_EVENTS 64
#define O_CREATE_M 1
#define O_LOCK_M 2
#define ID_START 200
#if !defined(NAME_MAX)
#define NAME_MAX 256
#endif

extern char *SOCK_NAME;
extern int sock_fd;
extern int epoll_fd;
extern struct epoll_event events[MAX_EVENTS];
extern size_t LOCK_ID;

typedef struct {
    int state;
    size_t response;
    size_t is_error;
    void *sendbuff;
    int sendbuff_end;
    int sendptr;
} conn_state_t;


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
int readFile(const char*, void **, size_t *);
int helper_receive(void **buff, size_t *size);
int readNFiles(int , const char *);
char* cwd();
char *get_full_path(char *, size_t );
int writeFile(const char *pathname, const char *dirname);
int appendToFile(const char* pathname, void *data, size_t size, const char* dirname);
int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);
#endif // !API_SOCK_H