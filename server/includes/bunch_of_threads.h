#ifndef BUNCH_OF_THREADS_H
#define BUNCH_OF_THREADS_H
#include <pthread.h>
#include <signal.h>
#include "files_s.h"
#include "new.h"

typedef struct threads_w {
    pthread_t *my_thr;
    pthread_cond_t cond;
    pthread_mutex_t LOCK_FD;
    icl_hash_t *STORE;
    cach_hash_t *CACHE;
    int num_thr;
    int created_thr;
    int *fds;
    int size_fds;
    int tail_fds;
    int pipe_done_fd[2];
    volatile sig_atomic_t *time_to_quit;
} Threads_w;
int create_them (Threads_w *);
int kill_them(Threads_w *);
#endif // !BUNCH_OG_THREADS_H