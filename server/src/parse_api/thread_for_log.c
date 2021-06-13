#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "../../includes/thread_for_log.h"
#include "../../includes/util.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>



/**
 * @PARAM: takes a struct that contains log file name, pipe and a variable to write
 * let the thread now that main thread has ended.
 * @RETURN: Nada
 */
void *log_to_file(void *arg_th){
    Arg_log_th *args = (Arg_log_th *)arg_th;
    char buff_pipe[BUFSIZE];
    memset(buff_pipe, 0, sizeof(buff_pipe));
    struct stat st;
    volatile int res =0;
    //Creates if not present a directory called logs
    if((errno = 0, stat(LOGS, &st)) == -1){
        if(errno == ENOENT){
            SYSCALL_PRINT(mkdir, res, mkdir(LOGS, 0744), "Could not create a logs directory.\n", NULL);
        } else {
            fprintf(stderr, "Calling stat in log_to_file. %s\n", strerror(errno));
            return NULL;
        }
    }
    errno =EINVAL;
    CHECK_EQ_RETURN(log_to_file, args->file_name, NULL, NULL, "File name to log all the good stuff is not specified, I'm out.\n", NULL);
    size_t size =strlen(LOGS)+strlen(args->file_name)+1;//directory plus filename
    char *path =  (char *)calloc(size, sizeof(*path));
    CHECK_EQ_RETURN(calloc, path, NULL, NULL,"Probably not enough memory, I'm out.\n", NULL);
    snprintf(path, size, "%s%s", LOGS, args->file_name);
    mode_t preced = umask(0033);
    //opens or creates file for logs
    FILE *fl_log = NULL;
    fl_log = fopen(path, "a+");
    CHECK_EQ_EXIT(fopen, fl_log, NULL, "Could not open or create log file, I'm out.\n", NULL);
    umask(preced);
    int fd_log = 0;
    SYSCALL_PRINT(fileno, fd_log, fileno(fl_log), "\033[1;31mATTENTION: \033[0;37mCould not get file descriptor.\n", NULL);
    free(path);
    struct tm r_tm;
    time_t now = time(NULL);
    struct tm lc_tm = *localtime_r(&now, &r_tm);
    char *user =  getenv("USERNAME");
    setvbuf(fl_log, NULL, _IONBF, 0);
    fprintf(fl_log, "<-------------------------USER: %s  DATE START: %d-%02d-%02d  %02d:%02d------------------------->\n",user, lc_tm.tm_year+1900, lc_tm.tm_mon+1, lc_tm.tm_mday, lc_tm.tm_hour, lc_tm.tm_min);
    fflush(fl_log);
    while(!args->sign || res != 0){
        if((errno = 0, res = read(args->pipe[READ], buff_pipe, BUFSIZE)) == -1) {
            fprintf(stderr, "\033[1;31mread has failed\033[0;37m:%s\n", strerror(errno));
            break;
        }
        fwrite(buff_pipe, sizeof(char), res, fl_log);
        fflush(fl_log);
    }
    close(args->pipe[READ]);
    lc_tm = *localtime_r(&now, &r_tm);
    fprintf(fl_log, "<-------------------------USER: %s  DATE END: %d-%02d-%02d  %02d:%02d------------------------->\n",user, lc_tm.tm_year+1900, lc_tm.tm_mon+1, lc_tm.tm_mday, lc_tm.tm_hour, lc_tm.tm_min);
    fprintf(stdout,"\033[1;36mLog  thread has ended.\033[0;37m\n");
    fflush(fl_log);
    fclose(fl_log);
    return NULL;
}
