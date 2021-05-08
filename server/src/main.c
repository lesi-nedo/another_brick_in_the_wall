#include <signal.h>
#include "../includes/parsing_conf.h"
#include "../includes/util.h"
#include <string.h>
#include "../includes/thread_for_log.h"
#include <errno.h>

#define READ 0
#define WRITE 1

int main (int argc, char **argv){
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
    Arg_log_th arg;
    arg.sign = 0;
    arg.file_name = all_settings[hash((unsigned char *)"LOG_FILE_NAME")%BASE_MOD%num_avail_settings].value_string;
    int res =0;
    SYSCALL_EXIT(pipe, res, pipe(arg.pipe), "This is embarrassing try to rerun me.\n", NULL);
    fflush(stdout);
    pthread_t th_logger = 0;
    //Creates a thread to manage all logs with the cal dprintf but be careful that the function gets blocked
    //if there are no readers and pipe is open.
    SYSCALL_EXIT(pthread_create, res, pthread_create(&th_logger, NULL, &log_to_file, (void *)&arg), "It is a shame, I failed.\n", NULL);
    while(res < 10){
        sleep(1);
        dprintf(arg.pipe[WRITE], "SUKKKKKKKKKKAAAA----%d\n", getpid());
        res++;
    }
    arg.sign = 1;
    close(arg.pipe[WRITE]);
    pthread_join(th_logger, NULL);
    return EXIT_SUCCESS;
}