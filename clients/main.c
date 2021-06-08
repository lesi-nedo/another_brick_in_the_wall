#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>
#include <ftw.h>
#include "../server/includes/api_sock.h"
#include "../server/includes/util.h"
#define EP_EVENTS 20

// b /home/nedo1993/Desktop/SOL_project/server/src/sockets/api_sock.c:

//! AFTER OPEN WITH OPTION O_CREATE O_CREATE | O_LOCK READ FROM SOCKET THE RESPONSE
//!SERVER MIGHT SEND A VICTIM! (Do not read if you pass only O_LOCK)
int fn(const char*fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf){
    if(tflag == FTW_F){
        char *path = realpath(fpath, NULL);
        int res =0;
        res = writeFile(path, "./libs/");
        if(res == -1){
            print_error("SOMETHING WENT WRONG, errno: %s\n", strerror(errno));
            return 1;
        }
        free(path);
    }
    return 0;
}

int main (int argc, char **argv){
    char path[] = {"../server/src/some_name"};
    // char rand_str[14];
    // char str_to_wrt[8046];
    char curr_dir[] = {"Devo fare  Il cazzo di sto progetto. Suka blajajajajaj asklalkss asssssssssssssssssssssssssssssssssssssssssssssssss sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssAAAA\n"};
    SOCK_NAME = path;
    // void *buff = NULL;
    size_t size = sizeof(curr_dir);
    int res = 0;
    struct timespec abs;
    char *buff = NULL;
    abs.tv_sec =1;
    abs.tv_nsec = 500000;
    res = openConnection(path, 30, abs);
    if(res==-1){
        closeConnection(path);
        exit(EXIT_FAILURE);
    }
    // chdir("./tests/");
    // rand_string(rand_str, 10);
    // rand_str[9]='.';
    // rand_str[10]='t';
    // rand_str[11]='x';
    // rand_str[12]='t';
    // rand_str[13]='\0';
    // FILE *fp = fopen(rand_str, "w");
    // rand_string(str_to_wrt, 8046);
    // fprintf(fp, "%s\n", str_to_wrt);
    // fflush(fp);
    // res = nftw("./tests", fn, 30, FTW_MOUNT | FTW_PHYS);
    if(res > 0) return -1;
    // printf("%zu\n", LOCK_ID);
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/rvflctKuE.txt", curr_dir, size, "./files/");
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/rvflctKuE.txt", curr_dir, size, "./files/");
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/AilcoCnnE.txt", curr_dir, size, "./files/");
    // res = readFile("/home/nedo1993/Desktop/SOL_project/clients/tests/rvflctKuE.txt", (void **)&buff, &size);
    // res=readNFiles(-10, "./files");
    LOCK_ID=202;
    res = lockFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt");
    // LOCK_ID=201;
    // res = unlockFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt");
    // res = closeFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt");
    // res = openFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt", O_LOCK_M|O_CREATE_M);
    // helper_receive((void **)&buff, &size);
    // res = readFile("/home/nedo1993/Desktop/SOL_project/clients/tests/zkwdqiztq.txt", (void**)&buff, &size);
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt", curr_dir, size, "./files/");
    // res = readFile("/home/nedo1993/Desktop/SOL_project/clients/tests/zkwdqiztq.txt", (void**)&buff, &size);
    // res = removeFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt");
    res = readFile("/home/nedo1993/Desktop/SOL_project/clients/tests/ADmCukzco.txt", (void**)&buff, &size);
    if (buff) printf("%s", buff);
    free(buff);
    // fclose(fp);
    close(sock_fd);
    return EXIT_SUCCESS;

}