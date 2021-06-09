#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>
#include <ftw.h>
#include "../server/includes/api_sock.h"
#include "../server/includes/util.h"
#define EP_EVENTS 20
#define w 0
#define W 1
#define D 2
#define r 3
#define f 4
#define d 5
#define t 6
#define l 7
#define u 8
#define c 9
#define R 10
#define h 11
#define p 12
#define NUM_ARGS 13
#define MAX_FD 30
#define H  "\033[1;33mUsage:\033[1;37m ./client [option] [args]\n\
            \033[1;35m-h\033[1;37m prints all acceptable commands.\n\
            \033[1;35m-f\033[1;37m socket name to connect to.\n\
            \033[1;35m-w\033[1;37m writes n files to server, if n=0 or not specified than writes all files in current and sub directories. option: [n=0]\n\
            \033[1;35m-W\033[1;37m list of files to write to the server.\n\
            \033[1;35m-D\033[1;37m folder where victims will be saved.\n\
            \033[1;35m-r\033[1;37m list of the file to be read from the server.\n\
            \033[1;35m-R\033[1;37m reads any file from the server. option [n=0]\n\
            \033[1;35m-d\033[1;37m folder where requested files will be saved.\n\
            \033[1;35m-d\033[1;37m folder where requested files will be saved.\n\
            \033[1;35m-t\033[1;37m delay between two requests.[optional].\n\
            \033[1;35m-l\033[1;37m lock file/s\n\
            \033[1;35m-u\033[1;37m  unlock file/s\n\
            \033[1;35m-c\033[1;37m  list of files to be removed from the server.\n\
            \033[1;35m-p\033[1;37m verbose.\n"
#define SUCCESS(...) print_success( __VA_ARGS__)
#define FAILURE(...) print_error(__VA_ARGS__)
#define API_CALL(p, fun, res, succ, fail, free_elem, ...) \
    res = fun(__VA_ARGS__); \
    if(p){  \
        printf("\033[1;96m"#fun"\033[1;37m:\n"); \
        if(res==-1){ \
            fail;  \
            free_elem; \
            exit(EXIT_FAILURE); \
        } \
        else succ; \
    }\
    if(res < 0){ \
        printf("\033[1;31mFailed.\033[0;37m\n");\
        exit(EXIT_FAILURE); \
    }
#define EXEC_COMD(COMD, ind, fun, api_name, save_file)\
    if(comd[COMD].command){\
        ind =0;\
        while(ind < comd[COMD].num_args){\
            char *path = NULL;\
            long dot = 0, size_str= 0, begin = 0;\
            size_str=strlen(comd[COMD].names[ind]);\
            comd[COMD].names[ind][size_str]='\0';\
            while(dot<= size_str){\
                if(comd[COMD].names[ind][dot] ==',' || comd[COMD].names[ind][dot] =='\0'){\
                    comd[COMD].names[ind][dot]='\0';\
                    path = realpath(comd[COMD].names[ind]+begin, NULL);\
                    if(path == NULL){\
                        printf("\033[1;96m"#api_name"\033[1;37m:\n"); \
                        FAILURE("Failed. \nerrno:%s\n", strerror(errno));\
                    }\
                    fun;\
                    begin =dot+1;\
                    save_file;\
                    free(path);\
                }\
                dot++;\
                msleep(comd[t].n);\
            }\
            ind++;\
        }\
    }
#define DEFAUL_SECONDS 1
#define DEFAULT_NANSEC 4000000

char *VICTIM_PLACE =NULL;
typedef struct commands {
    int command;
    long n;
    char **names;
    int num_args;
} Commands;
Commands comd[NUM_ARGS];
int COUNT_FILES=0;

//! AFTER OPEN WITH OPTION O_CREATE O_CREATE | O_LOCK READ FROM SOCKET THE RESPONSE
//!SERVER MIGHT SEND A VICTIM! (Do not read if you pass only O_LOCK)
/**
 * @brief this function is passed as argument to the nftw
 * @return 0 tells to nftw to keep going, value > 0 stops nftw.
 */
int fn(const char*fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf){
    if(tflag == FTW_F){
        char *path = realpath(fpath, NULL);
        int res =0;
        API_CALL(comd[p].command, writeFile, res, SUCCESS("sent file: %s. sent bytes: %d\n",\
        path, SIZE_FILE), FAILURE("could not sent %s\n\033[31merrno:\033[1;37m%s\033[0m\n", path, strerror(errno)), , path, VICTIM_PLACE);
        free(path);
        COUNT_FILES++;
        if(comd[w].n && comd[w].n == COUNT_FILES) return 1;
        msleep(comd[t].n);
    }
    return 0;
}

/**
 * @brief: this function was created for the command -r
 * @param file_name: name of file to be saved in the permanent memory
 * @param dirname: file will be saved here, if null than it will dump to dev/null
 * @param buff:buffer for data
 * @param size: size of data read from server
 * @return nada
 */

void save_file(char *file_name, const char *dirname, void *buff, size_t size){
    if(dirname == NULL){
        dirname = "/dev/";
        file_name ="null";
    }
    int res =0;
    char *curr_dir =cwd();
    if(curr_dir == NULL){

        printf("\033[1;96mreadFile\033[1;37m:\n");
        FAILURE("Failed. \nerrno:%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    res = chdir(dirname);
    if(res < 0){
        free(curr_dir);
        printf("\033[1;96mreadFile\033[1;37m:\n");
        FAILURE("Failed. \nerrno:%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    char *name = basename(file_name);
    FILE *file = fopen(name, "w");
    if(file == NULL){
        free(curr_dir);
        printf("\033[1;96mreadFile\033[1;37m:\n");
        FAILURE("Failed. \nerrno:%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s", (char*)buff);
    fclose(file);
    res = chdir(curr_dir);
    if(res < 0){
        free(curr_dir);
        printf("\033[1;96mreadFile\033[1;37m:\n");
        FAILURE("Failed. \nerrno:%s\n", strerror(errno));
    };
    free(curr_dir);
}

/**
 * @param comd: array of commands
 */
void _free_comd(Commands comd[NUM_ARGS]){
    for(size_t i =0; i<NUM_ARGS; i++)free(comd[i].names);
}

/**
 * @brief: with option -f it tries to connect to the server if specified -t will try again.
 * each option is save in an struct array with relative arguments.
 */
int main (int argc, char **argv){
    int opt=0;
    int res =0;
    // int n=0;
    void *buff = NULL;
    size_t size =0;
    struct timespec abs;
    abs.tv_sec = DEFAUL_SECONDS;
    abs.tv_nsec= DEFAULT_NANSEC;
    int ind =0;
    long num =0;
    for(size_t i=0; i<NUM_ARGS; i++){
        comd[i].command=0;
        comd[i].num_args=0;
        comd[i].n=0;
        comd[i].names=(char **)calloc(1, sizeof(char*));
        if(comd[i].names == NULL){
            print_error("Buddy go and get some memory, bye.\n", NULL);
            exit(EXIT_FAILURE);
        }
    }
    while ((opt = getopt(argc,argv, ":f:w:W:D:r:d:t:l:u:c:pRh")) != -1) {
        switch(opt){
            case 'h':
            if(comd[h].command == 1){
                print_error("you can pass only once the command \033[36mh\033[0m\n");
                _free_comd(comd);
                exit(EXIT_FAILURE);
            }
            comd[h].command=1;
            printf(H);
            break;
            case 'f':
            if(comd[f].command == 1){
               print_error("you can pass only once the command \033[36mf\033[0m\n");
                _free_comd(comd);
                exit(EXIT_FAILURE);
            }
            comd[f].command=1;
            SOCK_NAME=optarg;
            comd[f].names[comd[f].num_args]=optarg;
            comd[f].num_args++;
            opt = strlen(optarg);
            
            break;
            case 'w':
                comd[w].command=1;
                comd[w].names[comd[w].num_args]=optarg;
                comd[w].num_args++;
                if(argv[optind] && argv[optind][0] == 'n'){
                    if(argv[optind][1]== '='){
                        if(!isNumber((argv[optind]+2), &num)){
                            optind++;
                            comd[w].n=num;
                            break;
                        }
                    }
                    print_error("-w accepts dirname[n=number], what you passed is wrong.\n", NULL);
                    _free_comd(comd);
                    exit(EXIT_FAILURE);
                }
                break;
            case'W':
                comd[W].command=1;
                comd[W].names[comd[W].num_args]=optarg;
                comd[W].num_args++;
                break;
            case 'D':
                comd[D].command=1;
                comd[D].names[comd[D].num_args]=optarg;
                comd[D].num_args++;
                break;
            case 'r':
                comd[r].command=1;
                comd[r].names[comd[r].num_args]=optarg;
                comd[r].num_args++;
                break;
            case 'R':
                comd[R].command=1;
                if(argv[optind] && argv[optind][0] == 'n'){
                    if(argv[optind][1]== '='){
                        if(!isNumber((argv[optind]+2), &num)){
                            optind++;
                            comd[R].n=num;
                            break;
                        }
                    }
                    print_error("-R accepts [n=number], what you passed is wrong.\n", NULL);
                    _free_comd(comd);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                comd[d].command=1;
                comd[d].names[comd[d].num_args]=optarg;
                comd[d].num_args++;
                break;
            case 't':
                comd[t].command=1;
                if(isNumber(optarg, &num)){
                    print_error("t needs a positive number,  -h for usage\n", optopt);
                    _free_comd(comd);
                    exit(EXIT_FAILURE);
                }
                comd[t].n=num;
                break;
            case 'l':
                comd[l].command=1;
                comd[l].names[comd[l].num_args]=optarg;
                comd[l].num_args++;
                break;
            case 'u':
                comd[u].command=1;
                comd[u].names[comd[u].num_args]=optarg;
                comd[u].num_args++;
                break;
            case 'c':
                comd[c].command=1;
                comd[c].names[comd[c].num_args]=optarg;
                comd[c].num_args++;
                break;
            case 'p':
                if(comd[p].command == 1){
                    print_error("you can pass only once the command \033[36mp\033[0m\n");
                    _free_comd(comd);
                    exit(EXIT_FAILURE);
                }
                comd[p].command=1;
                break;
            case '?':
                print_error("Unknown command %c,  -h for usage\n", optopt);
                _free_comd(comd);
                exit(EXIT_FAILURE);
            break;
            case ':':
                print_error("%c needs an argument,  -h for usage\n", optopt);
                _free_comd(comd);
                exit(EXIT_FAILURE);

        }
    }
    if(comd[D].command){
            if(!comd[W].command && !comd[w].command){
                print_error("option \033[1;31m-D\033[1;37m must be used with \033[1;36m-W\033[1;37m or \033[1;36m-w\033[1;37m\n", NULL);
                _free_comd(comd);
                exit(EXIT_FAILURE);
            }
        }
    if(comd[d].command){
        if(!comd[r].command && !comd[R].command){
            print_error("option \033[1;31m-d\033[1;37m must be used with \033[1;36m-r\033[1;37m or \033[1;36m-R\033[1;37m\n", NULL);
            _free_comd(comd);
            exit(EXIT_FAILURE);
        }
    }
    API_CALL(comd[p].command, openConnection, res, SUCCESS("Connection established.\n", NULL),
    FAILURE("Could not established the connection\n", NULL), _free_comd(comd), SOCK_NAME, comd[t].n, abs);
    LOCK_ID=201;

    if(comd[D].command){
        VICTIM_PLACE = comd[D].names[0];
    }
    if(comd[w].command){
        nftw(comd[w].names[0], fn, MAX_FD, FTW_MOUNT | FTW_PHYS);
    }
    msleep(comd[t].n);
    if(comd[d].command){
        VICTIM_PLACE=comd[d].names[0];

    }
    if(comd[R].command){
         API_CALL(comd[p].command, readNFiles, res, SUCCESS("bytes read: %d\n", SIZE_FILE), \
        FAILURE("failed.\nerrno:%s\n",strerror(errno)),_free_comd(comd), comd[R].n, VICTIM_PLACE);
    }
    msleep(comd[t].n);
    //sorry if this is not as friendly as could be. One macro is for managing files passed as arguments
    //It takes the command, ind is used if has been passed multiple command. For example if there were two -w -w with files
    //than it will be save at index 0, 1. ind is used to read all files associated to the single command
    //API_CALL it check if -p was passed than calls the function and manages the result accepts two macros for writing massages with
    //variable arguments.
    EXEC_COMD(W, ind,  API_CALL(comd[p].command, writeFile, res, SUCCESS("sent file: %s.sent bytes: %d\n",\
        path, SIZE_FILE), FAILURE("could not sent %s\n\033[31merrno:\033[1;37m%s\033[0m\n", path, strerror(errno)), , path, VICTIM_PLACE), writeFile, NULL);
        
    msleep(comd[t].n);
    
    EXEC_COMD(r, ind, API_CALL(comd[p].command, readFile, res, SUCCESS("Read file: %s. Bytes: %zu\n", path, size),\
        FAILURE("Read failed.\nerrno:%s\n", strerror(errno)), _free_comd(comd), path, &buff, &size), readFile, \
        save_file(path, comd[d].names[0]?comd[d].names[0]: NULL, buff, size));


    msleep(comd[t].n);
    
    EXEC_COMD(l, ind, API_CALL(comd[p].command, lockFile, res, SUCCESS("File locked:%s\n", path),   \
        FAILURE("failed lockFile.\nerrno:%s\n", strerror(errno)), _free_comd(comd), path), lockFile, NULL);
        
    msleep(comd[t].n);
    
    EXEC_COMD(u, ind, API_CALL(comd[p].command, unlockFile, res, SUCCESS("File unlocked:%s\n", path),   \
        FAILURE("failed unlockFile.\nerrno:%s\n", strerror(errno)), _free_comd(comd), path), unlockFile, NULL);

    msleep(comd[t].n);

    EXEC_COMD(c, ind, API_CALL(comd[p].command, removeFile, res, SUCCESS("File removed:%s\n", path),   \
        FAILURE("failed removeFile.\nerrno:%s\n", strerror(errno)), _free_comd(comd), path), removeFile, NULL);
        
    if(buff) free(buff);
    _free_comd(comd);
    closeConnection(SOCK_NAME);
    return EXIT_SUCCESS;

}
