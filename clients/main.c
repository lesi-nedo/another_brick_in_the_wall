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


int fn(const char*fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf){
    if(tflag == FTW_F){
        char *path = realpath(fpath, NULL);
        int res =0;
        res = writeFile(path, NULL);
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
    // char str_to_wrt[846];
    // char curr_dir[] = {"Devo fare  Il cazzo di sto progetto. Suka blajajajajaj asklalkss asssssssssssssssssssssssssssssssssssssssssssssssss sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss\n"};
    SOCK_NAME = path;
    // void *buff = NULL;
    // size_t size = sizeof(curr_dir);
    int res = 0;
    struct timespec abs;
    // char *buff = NULL;
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
    // rand_string(str_to_wrt, 846);
    // fprintf(fp, "%s\n", str_to_wrt);
    // fflush(fp);
    nftw("./tests", fn, 30, FTW_MOUNT | FTW_PHYS);
    // curr_dir = get_full_path("my_id.txt", 9);
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/AilcoCnnE.txt", curr_dir, size, "./files/");
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/AilcoCnnE.txt", curr_dir, size, "./files/");
    // res = appendToFile("/home/nedo1993/Desktop/SOL_project/clients/tests/AilcoCnnE.txt", curr_dir, size, "./files/");

    // res = readFile("/home/nedo1993/Desktop/SOL_project/clients/tests/AilcoCnnE.txt", (void **)&buff, &size);
    // if (buff) printf("%s", buff);
    // free(buff);
    // fclose(fp);
    close(sock_fd);
    return EXIT_SUCCESS;

}