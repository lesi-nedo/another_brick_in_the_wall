#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "../server/includes/api_sock.h"
#include "../server/includes/util.h"
#define EP_EVENTS 20




int main (int argc, char **argv){
    char path[] = {"../server/src/some_name"};
    char rand_str[5];
    SOCK_NAME = path;
    int res = 0;
    struct timespec abs;
    abs.tv_sec =1;
    abs.tv_nsec = 500000;
    res = openConnection(path, 30, abs);
    if(res==-1){
        closeConnection(path);
        exit(EXIT_FAILURE);
    }
    rand_string(rand_str, 5);
    printf("%s\n", rand_str);
    res = openFile(rand_str, O_CREATE_M);
    if(res==-1){
        closeConnection(path);
        exit(EXIT_FAILURE);
    }
    printf("%zd\n", LOCK_ID);
    close(sock_fd);
    return EXIT_SUCCESS;

}