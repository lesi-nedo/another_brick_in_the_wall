#include <signal.h>
#include "../includes/parsing_conf.h"
#include "../includes/util.h"
#include <string.h>
#include "../includes/thread_for_log.h"
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include "../includes/new.h"

#define READ 0
#define WRITE 1
Arg_log_th ARG_LOG_TH;
cach_hash_t *MY_CACHE;
icl_hash_t *FILES_STORAGE;
static volatile sig_atomic_t sig_teller = 0;

static void sighan(int sig){
    switch(sig){
        case SIGINT:
        case SIGQUIT:
            sig_teller = 1;
            break;
        case SIGHUP:
            sig_teller = 2;
            break;
        default:
            perror("Something went terribly wrong, ciao");
    }
}

void cache_init_sett(Server_conf settings[], struct Cache_settings *sett){
    for(size_t i=1; i<num_avail_settings; i++){
        unsigned long hashed_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        switch(i){
            case 1:
                sett->START_INI_CACHE = settings[hashed_val].value;
                i = 4;
                break;
            case 5:
                sett->NUM_GROUP = settings[hashed_val].value;
                break;
            case 6:
                sett->CACHE_RANGE = settings[hashed_val].value;
                break;
            case 7:
                sett->EXTRA_CACHE_SPACE = settings[hashed_val].value_float;
                break;
            default:
            printf("%zd --- ", i);
                printf("\033[1;35mSomething went terribly wrong.\033[1;37m\n");
                break;
        }
    }//NU_GROUP has to b less the startinicache
}

int main (int argc, char **argv){
    int res =0;
    //Ignoring EPIPE for all threads and masking all signals
    sigset_t set, old_m;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&set);
    sigaddset(&set, SIGINT); 
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGPIPE);
    SYSCALL_EXIT(pthread_sigmask, res, pthread_sigmask(SIG_BLOCK, &set, &old_m), "Sorry,bye.\n", NULL);
    sa.sa_handler = SIG_IGN;
    SYSCALL_EXIT(sigaction, res, sigaction(SIGPIPE, &sa, NULL), "Sorry,bye.\n", NULL);
    sa.sa_handler = &sighan;
    SYSCALL_EXIT(sigaction, res, sigaction(SIGINT, &sa, NULL), "Sorry,bye.\n", NULL);
    SYSCALL_EXIT(sigaction, res, sigaction(SIGQUIT, &sa, NULL), "Sorry,bye.\n", NULL);
    SYSCALL_EXIT(sigaction, res, sigaction(SIGHUP, &sa, NULL), "Sorry,bye.\n", NULL);
     //parsing conf file
    char path_conf[] = {"../config.txt"};
    struct Cache_settings cach_set;
    Server_conf  all_settings[num_avail_settings];
    init_serv_conf(all_settings);
    init_settings_arr(all_settings);
    parse_file(path_conf, all_settings);
    cache_init_sett(all_settings, &cach_set);
    MY_CACHE = cach_hash_create(cach_set, get_next_index);
    int buckets_fl = (int) cach_set.START_INI_CACHE+cach_set.START_INI_CACHE*0.4;
    FILES_STORAGE = icl_hash_create(buckets_fl, triple32, string_compare);
    //Setting new set
    SYSCALL_EXIT(pthread_sigmask, res, pthread_sigmask(SIG_SETMASK, &old_m, NULL), "Sorry,bye.\n", NULL);
   
    // icl_hash_t FILES_STORAGE;
    //START_INI_CACHE is NUM_FILES
    // cach_hash_t MY_CACHE;
    ARG_LOG_TH.sign = 0;
    ARG_LOG_TH.file_name = all_settings[hash((unsigned char *)"LOG_FILE_NAME")%BASE_MOD%num_avail_settings].value_string;
    SYSCALL_EXIT(pipe, res, pipe(ARG_LOG_TH.pipe), "This is embarrassing try to rerun me.\n", NULL);
    fflush(stdout);
    pthread_t th_logger = 0;
    //Creates a thread to manage all logs
    //if there are no readers and pipe is open dprinf gets stuck.
    //or buff gets full //!BE CAREFUL
    SYSCALL_EXIT(pthread_create, res, pthread_create(&th_logger, NULL, &log_to_file, (void *)&ARG_LOG_TH), "It is a shame, I failed.\n", NULL);
    while(res < 40){
        dprintf(ARG_LOG_TH.pipe[WRITE], "SUKKKKKKKKKKAAAA----%d\n", getpid());
        res++;
    }
    //TODO: YOU NEED TO UPDATE A FLAG IN THE HASH TABLE TO 1 SO IT TELLS THAT FILE IS PRESENT 
    //TODO: BUT NOT YET ALLOCATED
    ARG_LOG_TH.sign = 1;
    close(ARG_LOG_TH.pipe[WRITE]);
    free(all_settings[hash((unsigned char *)"SOCKET_NAME")%BASE_MOD%num_avail_settings].value_string);
    cach_hash_destroy(MY_CACHE);
    icl_hash_destroy(FILES_STORAGE, NULL, NULL);
    pthread_join(th_logger, NULL);
    return EXIT_SUCCESS;
}