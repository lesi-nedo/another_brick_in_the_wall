#include "../../includes/bunch_of_threads.h"
#include <errno.h>
#include<stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include "../../includes/util.h"
#include <string.h>
#include "../../includes/api_sock.h"
#include "../../includes/conn.h"
#include <sys/epoll.h>

int _open_helper(int fd, char **path_ret, volatile sig_atomic_t *time_to_quit){
    size_t length = 0;
    int n = 0;
    char *path = NULL;
    if((n=readn(fd, &length, sizeof(length))) == -1){
        if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
        return 0;
    }
    assert(length > 0);
    path = (char *)calloc(length, sizeof(*path));
    if(!path){
        print_error("Houston we have a problem, we are out of memory.\nerrno:%s\n", strerror(errno));
        return -1;
    }
    if((n=readn(fd, path, length)) == -1){
        if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
        return 0;
    }
    errno =0;
    path[length-1] = '\0';
    *path_ret = path;
    return 1;
}

int _write_resp(int fd, volatile sig_atomic_t *time_to_quit, size_t resp){
    int ret_w = 0;
    int n = 0;
    if((n=writen(fd, &resp, sizeof(resp), &ret_w)) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
            global_state[fd].state = EPOLLOUT;
            global_state[fd].response = resp;
            return 0;
    }
    return 1;
}

int open_f_l(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER){
    int res = 0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    free(path);
    if(curr == NULL || curr->O_LOCK){
        return 0;
    }
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    curr->OWNER = *OWNER == 0 ? (size_t) time(NULL): *OWNER;
    curr->open = 1;
    curr->O_LOCK= 1;
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    *OWNER = curr->OWNER;
    return 1;
}

int open_f_c(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER){
    pointers victim;
    memset(&victim, 0, sizeof(victim));
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    icl_entry_t *curr = icl_hash_insert(STORE, (void *)path, NULL, 0, &victim);
    if((curr == NULL && errno == EEXIST)){
        free(path);
        return 0;
    }
    if(curr == NULL){ 
        free(path);
        return -1;
    }
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    curr->OWNER = *OWNER == 0? (size_t)time(NULL): *OWNER;
    curr->open = 1;
    curr->O_LOCK = 0;
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    *OWNER = curr->OWNER;
    return 1;
}

int (*fun_ptr[NUM_AP])(icl_hash_t *, cach_hash_t *, int, void **, volatile sig_atomic_t *, size_t*) ={open_f_l, open_f_c};




static void *do_magic_you_bi(void *info_w){
    Threads_w *info = (Threads_w *)info_w;
    int connfd = 0;
    int ret =0;
    // void *to_proc_data = NULL;
    // int size_data =0;
    size_t NOT_OK = NOT_OK_M;
    size_t op_OWNER[2];
    int n = 0;
    while(*(info->time_to_quit) != 1){
        while((errno = 0, n=readn(info->pipe_ready_fd[READ], &connfd, sizeof(int))) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK ) || *(info->time_to_quit) == 1) break;
            sched_yield();
        }
        if(n == 0) break;
        if(*(info->time_to_quit) == 1) break;
        if(global_state[connfd].state & EPOLLIN){
            if((n=readn(connfd, op_OWNER, sizeof(op_OWNER))) == -1){
                if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), &ret)) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                }
                continue;
            }
            if(*(info->time_to_quit) == 1) break;
            if(n == 0) break;
            if(op_OWNER[0] < 0 || op_OWNER[0] > NUM_AP){
                if(writen(connfd, &NOT_OK, sizeof(NOT_OK), &ret) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    global_state[connfd].state = EPOLLOUT;
                    global_state[connfd].response = NOT_OK;
                    while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), &ret)) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    }
                    continue;
                }
            } else {
                ret = fun_ptr[op_OWNER[0]](info->STORE, info->CACHE, connfd, NULL, info->time_to_quit, &op_OWNER[1]);
                if(ret == 1){
                    _write_resp(connfd, info->time_to_quit, op_OWNER[1]);
                } else {
                    _write_resp(connfd, info->time_to_quit, NOT_OK);
                    if(ret == -1) goto EXIT;
                }
            }
        } else {
            if(*(info->time_to_quit) == 1) break;
            if(global_state[connfd].response > -1){
                if(writen(connfd, &global_state[connfd].response, sizeof(global_state[connfd].response), &ret) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    global_state[connfd].state = EPOLLOUT;
                    while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), &ret)) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    }
                    continue;
                }
                global_state[connfd].response = -1;
                global_state[connfd].state = EPOLLIN;
                global_state[connfd].sendptr = 0;
                global_state[connfd].sendbuff_end = 0;
            } else {
                if(*(info->time_to_quit) == 1) goto EXIT;
                #ifdef DEBUG
                assert(global_state[connfd].sendbuff);
                #endif // DEBUG
                size_t left = global_state[connfd].sendbuff_end - global_state[connfd].sendptr;
                ret = 0;
                if((writen(connfd, ((uint8_t*)global_state[connfd].sendbuff)+global_state[connfd].sendptr, left, &ret)) < 0 || ret > 0){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) break;
                    global_state[connfd].state = EPOLLOUT;
                    if(n > 0)  global_state[connfd].sendptr += (left-ret);
                    while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), &ret)) == -1){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    }
                    continue;
                }
                global_state[connfd].state = EPOLLIN;
                global_state[connfd].sendptr = 0;
                global_state[connfd].sendbuff_end = 0;
                global_state[connfd].sendbuff = NULL;
            }
        }
        if(*(info->time_to_quit) == 1) break;
        while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), NULL)) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
        }
    }
    //TODO: SIGHUP HANDLE
    EXIT:
    close(connfd);
    return NULL;
}

int kill_those_bi(Threads_w *info){
    if(info == NULL){
        errno = EINVAL;
        return -1;
    }
    for(size_t i = 0; i < info->created_thr; i++){
        if(pthread_join(info->my_bi[i], NULL) != 0){
            errno = EFAULT;
            return -1;
        }
    }
    close(info->pipe_ready_fd[READ]);
    close(info->pipe_ready_fd[WRITE]);
    close(info->pipe_done_fd[WRITE]);
    close(info->pipe_done_fd[READ]);
    free(info->my_bi);
    return 0;
}

int create_them (Threads_w *info){
    if(info->num_thr < 0){
        errno = EINVAL;
        return -1;
    }
    info->created_thr = 0;
    info->max_buff = PIPE_BUF;
    info->my_bi = (pthread_t*)malloc(sizeof(pthread_t)*info->num_thr);
    if(info->my_bi ==NULL){
        fprintf(stderr, "Damn boy, no more memory.\n");
        return -1;
    }
    for(size_t i = 0; i < info->num_thr; i++){
        if(pthread_create(&(info->my_bi[i]), NULL, do_magic_you_bi, (void*)info) != 0){
            kill_those_bi(info);
            errno = EFAULT;
            return -1;
        }
        info->created_thr++;
    }
    return 0;
}