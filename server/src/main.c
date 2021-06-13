#include <signal.h>
#include "../includes/parsing_conf.h"
#include "../includes/util.h"
#include "../includes/conn.h"
#include <string.h>
#include "../includes/thread_for_log.h"
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include "../includes/new.h"
#include "../includes/api_sock.h"
#include "../includes/bunch_of_threads.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

Arg_log_th ARG_LOG_TH;
cach_hash_t *MY_CACHE;
icl_hash_t *FILES_STORAGE;
long FILE_SIZE;
static volatile sig_atomic_t sig_teller = 0;

/**
 * @brief signal handler
 * @param sig: signal that we want to manage.
 */
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

/**
 * @brief: this function is for passing arguments to cach_hash_create
 * @param setting: this array has all settings parsed from .conf file
 * @param sett: where will be save relative settings to cache.
 * @return Nada.
 */
void cache_init_sett(Server_conf settings[], struct Cache_settings *sett){
    for(size_t i=0; i<num_avail_settings; i++){
        unsigned long hashed_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        switch(i){
            case 0:
                sett->SPACE_AVAILABLE = settings[hashed_val].value;
                break;
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
                printf("\033[1;35mSomething went terribly wrong.\033[1;37m\n");
                break;
        }
    }//NU_GROUP has to b less then start_ini_cache
}

/**
 * @brief: creates log thread, worker threads. Main thread communicates
 * between workers with two pipes one for ready fd and from one main reads fd that has been served.
 * But one thread keeps serving a client till it receives EAGAIN or EWOULDBLOCK from read or write
 * then it inserts into pipe.
 * Each workers dumps into pipe all logs than a thread that only reads from pipe and writes to the file does his job.
 * I decide to experiment a little bit so each new fd is non blocking, my intent was to make sort of asynchronous response but,
 * it turn out to be quite hard but I kept the non blocking fd
 */

int main (int argc, char **argv){
    int res =0;
    int color = 35;
    int total_con = 0;
    int listener_fd = 0;;
    int ep_fd = 0;
    Threads_w my_bi;
    struct epoll_event *events = NULL;
    struct epoll_event accept_event={0};
    struct epoll_event pipe_event={0};


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
    char path_conf[] = {"config.txt"};
    struct Cache_settings cach_set;
    Server_conf  all_settings[num_avail_settings];
    init_serv_conf(all_settings);
    init_settings_arr(all_settings);
    parse_file(path_conf, all_settings);
    cache_init_sett(all_settings, &cach_set);
    //creating cache and file store
    MY_CACHE = cach_hash_create(cach_set, get_next_index);
    int buckets_fl = (int) cach_set.START_INI_CACHE+cach_set.START_INI_CACHE*cach_set.EXTRA_CACHE_SPACE;
    FILES_STORAGE = icl_hash_create(buckets_fl, hash_pjw, string_compare);
    if(FILES_STORAGE == NULL || MY_CACHE == NULL){
        exit(EXIT_FAILURE);
    }
    FILES_STORAGE->MAX_SPACE_AVAILABLE= cach_set.SPACE_AVAILABLE;
    my_bi.num_thr = all_settings[hash(available_settings[2])%BASE_MOD%num_avail_settings].value;
    my_bi.time_to_quit = &sig_teller;
    my_bi.STORE = FILES_STORAGE;
    my_bi.CACHE = MY_CACHE;
    SOCK_NAME = all_settings[hash(available_settings[3])%BASE_MOD%num_avail_settings].value_string;
    unlink(SOCK_NAME);
    listener_fd = ini_sock(SOCK_NAME);
    res = non_blocking(listener_fd);
    if(res == -1 || listener_fd == -1){
        goto THATS_ALL_FOLKS;
    }
    ep_fd = epoll_create1(0);
    if(ep_fd < 0){
        res = errno;
        goto THATS_ALL_FOLKS;
    }
    accept_event.data.fd = listener_fd;
    accept_event.events = EPOLLIN;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, listener_fd, &accept_event) < 0){
        goto THATS_ALL_FOLKS;
    }
    //Setting new set
    if((res = pthread_sigmask(SIG_SETMASK, &old_m, NULL)) != 0){
        goto THATS_ALL_FOLKS;
    }
   
    ARG_LOG_TH.sign = 0;
    ARG_LOG_TH.file_name = all_settings[hash((unsigned char *)"LOG_FILE_NAME")%BASE_MOD%num_avail_settings].value_string;
    if(pipe(ARG_LOG_TH.pipe) == -1 || pipe2(my_bi.pipe_ready_fd, O_NONBLOCK) == -1 || pipe2(my_bi.pipe_done_fd, O_NONBLOCK)){
        res = errno;
        goto THATS_ALL_FOLKS;
    }
    pipe_event.data.fd = my_bi.pipe_done_fd[READ];
    pipe_event.events = EPOLLIN;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, my_bi.pipe_done_fd[READ], &pipe_event) < 0){
        goto THATS_ALL_FOLKS;
    }
    pthread_t th_logger = 0;
    //Creates a thread to manage all logs
    //if there are no readers and pipe is open dprinf gets stuck.
    //or buff gets full //!BE CAREFUL
    if((res=pthread_create(&th_logger, NULL, &log_to_file, (void *)&ARG_LOG_TH)) != 0){
        goto THATS_ALL_FOLKS;
    }
    res = create_them(&my_bi);
    if(res == -1){
        goto THATS_ALL_FOLKS;
    }
    events = (struct epoll_event *) calloc(MAX_FDS, sizeof(struct epoll_event));
    if(events==NULL){
        res = errno;
        goto THATS_ALL_FOLKS;
    }
    while(sig_teller != 1 && ( sig_teller==0 || (sig_teller== 2 && total_con > 0))){
        //THESE PRINTS MAKES MY CODE REALLY SLOW AND DELAY SIGNAL
        // I DON'T KNOW WHY
        // printf("\033[1;31mWAITING    \033[0;37m\033[s\n");
        // printf("\033[1A");
        int nready = epoll_wait(ep_fd, events, MAX_FDS, -1);
        for(int i =0; i < nready; i++){
            // printf("\033[1;%dmI'M RUNNING\033[0;37m\033[s\n", color);
            // printf("\033[1A");
            color = color ==35? 36 : 35;
            int new_sc = 0;
            if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
            !(events[i].events & EPOLLIN)){
                dprintf(ARG_LOG_TH.pipe[WRITE], "CLOSED: %d\n", events[i].data.fd);
                // print_error("Hi sweetie, I just want to let you know that epoll_wait failed on a file descriptor. <3 \n", NULL);
                total_con--;
                if(epoll_ctl(ep_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) <0){
                    res = errno;
                    goto THATS_ALL_FOLKS;
                }
                close(events[i].data.fd);
                continue;
            }
            if(events[i].data.fd == listener_fd){
                if((new_sc =accept(listener_fd, (struct sockaddr*)NULL, NULL)) < 0){
                    if(errno != EAGAIN || errno != EWOULDBLOCK){
                        print_error("With a broken heart I have to tell I have failed you, goodbye\n", NULL);
                        res = errno;
                        goto THATS_ALL_FOLKS;
                    }
                }
                if(sig_teller != 0){
                    close(new_sc);
                    continue;
                }
                if(new_sc > MAX_FDS){
                    print_error("I can't no more honey. Did not add the new one.\n", NULL);
                    continue;
                }
                non_blocking(new_sc);
                dprintf(ARG_LOG_TH.pipe[WRITE], "New: %d\n", new_sc);
                struct epoll_event event = {0};
                event.data.fd = new_sc;
                event.events = EPOLLIN;
                total_con++;
                if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, new_sc, &event) < 0){
                    print_error("Nothing personal it's just business.\n", NULL);
                    res = errno;
                    goto THATS_ALL_FOLKS;
                }
            } else if(events[i].data.fd == my_bi.pipe_done_fd[READ]){
                int to_add_fd = 0;
                int n =0;
                while((n = read(my_bi.pipe_done_fd[READ], &to_add_fd, sizeof(int)))>0){
                    struct epoll_event event = {0};
                    event.data.fd = to_add_fd;
                    event.events = EPOLLIN;
                    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, to_add_fd, &event) < 0){
                        print_error("Hey dandy, don't be mad but I ran into a problem.\nerrno: %s\n", strerror(errno));
                        res = errno;
                        goto THATS_ALL_FOLKS;
                    }
                }
                if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
                    print_error("Hasta luego, loco\n", NULL);
                    res = errno;
                    goto THATS_ALL_FOLKS;
                }
            } else{
                res =writen(my_bi.pipe_ready_fd[WRITE], &events[i].data.fd, sizeof(events[i].data.fd), &sig_teller);
                if(res < 0){
                    print_error("Our path ends here, have a nice life.\nerrno: %s\n",strerror(errno));
                    res = errno;
                    goto THATS_ALL_FOLKS;
                }
                if(epoll_ctl(ep_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) < 0){
                    res = errno;
                    goto THATS_ALL_FOLKS;
                }
            }
        }
        if(errno != EINTR && nready == -1){
            goto THATS_ALL_FOLKS;
        }
    }

THATS_ALL_FOLKS:
    sig_teller = 1;
    dprintf(ARG_LOG_TH.pipe[WRITE],"TOTAL FILES IN STORE: %ld\n", FILES_STORAGE->nentries);
    sched_yield();
    ARG_LOG_TH.sign = 1;
    close(ARG_LOG_TH.pipe[WRITE]);
    kill_those_bi(&my_bi);
    char path_logs[] ={"../logs"};
    res = chdir(path_logs);
    if(res < 0){
        print_error("Dump files at current location.\n", NULL);
    }
    FILE *fl_stor = fopen("store_dump.txt", "a");
    FILE *fl_cach = fopen("cache_dump.txt", "a");
    if(fl_stor != NULL && fl_cach != NULL){
        cach_hash_dump(fl_cach, MY_CACHE);
        icl_hash_dump(fl_stor, FILES_STORAGE);
        fclose(fl_stor);
        fclose(fl_cach);
    } else {
        if(fl_stor) fclose(fl_stor);
        if(fl_cach) fclose(fl_cach);
    }
    printf("\033[1;95mMaximum number of files: \033[1;37m %ld\033[0;37m\n", FILES_STORAGE->max_files);
    printf("\033[1;35mMaximum number of bytes: \033[1;37m %lld\033[0;37m\n", FILES_STORAGE->max_bytes);
    printf("\033[1;35mCurrent number of bytes: \033[1;37m %lld\033[0;37m\n", FILES_STORAGE->total_bytes);
    printf("\033[1;35mNumber of times replacement algorithm was called: \033[1;37m %ld\033[0;37m\n", FILES_STORAGE->total_victims);
    print_storage(FILES_STORAGE);
    cach_hash_destroy(MY_CACHE);
    icl_hash_destroy(FILES_STORAGE, free, free);
    free(events);
    pthread_join(th_logger, NULL);
    chdir("../server");
    unlink(SOCK_NAME);
    for(size_t i = 0; i< num_avail_settings; i++){
            if(all_settings[i].str_or_int == 0){
                free(all_settings[i].value_string);
            }
    }
    print_error("Have a wonderful day, sir. Error: %s\n", strerror(res));
    return errno;
}