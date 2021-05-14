#include <signal.h>
#include "../includes/parsing_conf.h"
#include "../includes/util.h"
#include <string.h>
#include "../includes/thread_for_log.h"
#include <errno.h>
#include <sched.h>

#define READ 0
#define WRITE 1
Arg_log_th ARG_LOG_TH;

int main (int argc, char **argv){
    srand(time(NULL));
    //Ignoring EPIPE for all threads and masking all signals
    sigset_t set;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigemptyset(&set);
    //Setting new set
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    //parsing conf file
    char path_conf[] = {"../config.txt"};
    Server_conf  all_settings[num_avail_settings];
    init_serv_conf(all_settings);
    init_settings_arr(all_settings);
    parse_file(path_conf, all_settings);
    icl_hash_t FILES_STORAGE;
    //START_INI_CACHE is NUM_FILES
    cach_hash_t MY_CACHE;
    ARG_LOG_TH.sign = 0;
    ARG_LOG_TH.file_name = all_settings[hash((unsigned char *)"LOG_FILE_NAME")%BASE_MOD%num_avail_settings].value_string;
    int res =0;
    SYSCALL_EXIT(pipe, res, pipe(ARG_LOG_TH.pipe), "This is embarrassing try to rerun me.\n", NULL);
    fflush(stdout);
    pthread_t th_logger = 0;
    //Creates a thread to manage all logs
    //if there are no readers and pipe is open dprinf gets stuck.
    //or buff gets full //!BE CAREFUL
    SYSCALL_EXIT(pthread_create, res, pthread_create(&th_logger, NULL, &log_to_file, (void *)&ARG_LOG_TH), "It is a shame, I failed.\n", NULL);
    while(res < 10){
        sleep(1);
        dprintf(ARG_LOG_TH.pipe[WRITE], "SUKKKKKKKKKKAAAA----%d\n", getpid());
        res++;
    }
    //TODO: YOU NEED TO UPDATE A FLAG IN THE HASH TABLE TO 1 SO IT TELLS THAT FILE IS PRESENT 
    //TODO: BUT NOT YET ALLOCATED
    ARG_LOG_TH.sign = 1;
    close(ARG_LOG_TH.pipe[WRITE]);
    free(all_settings[hash((unsigned char *)"SOCKET_NAME")%BASE_MOD%num_avail_settings].value_string);
    pthread_join(th_logger, NULL);
    return EXIT_SUCCESS;
}