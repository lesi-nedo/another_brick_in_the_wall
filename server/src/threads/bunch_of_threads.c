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
//REMEMBER 1 is ok, 0 the request was not ok, -1 fatal error -2 Buffer is full send request for EPOLLOUT
int _open_helper(int fd, char **path_ret, volatile sig_atomic_t *time_to_quit){
    size_t length = 0;
    int n = 0;
    char *path = NULL;
    while((errno=0,n=readn(fd, &length, sizeof(length))) == -1){
        if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
    }
    assert(length > 0);
    path = (char *)calloc(length, sizeof(*path));
    if(!path){
        print_error("Houston we have a problem, we are out of memory.\nerrno:%s\n", strerror(errno));
        return -1;
    }
    while((errno=0,n=readn(fd, path, length)) == -1){
        if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
    }
    errno =0;
    path[length-1] = '\0';
    *path_ret = path;
    return 1;
}

int _save_data(int fd, size_t ret_w, void *data){
    int old =global_state[fd].sendbuff_end-global_state[fd].sendptr;
    void *temp = NULL;
    #ifdef DEBUG
    assert(old < global_state[fd].sendbuff_end);
    #endif // DEBUG
    global_state[fd].state = EPOLLOUT;
    global_state[fd].sendbuff_end +=ret_w;
    temp = calloc((global_state[fd].sendbuff_end-global_state[fd].sendptr), sizeof(u_int8_t));
    if(temp == NULL) return -1;
    memcpy(temp, ((char*)global_state[fd].sendbuff+global_state[fd].sendptr), old+1);
    memcpy((char*)temp+global_state[fd].sendptr+old, data, ret_w);
    if(global_state[fd].sendbuff) free(global_state[fd].sendbuff);
    global_state[fd].sendbuff = temp;
    global_state[fd].sendptr = 0;
    return -2;
}

int _send_helper(int fd, void *data, size_t ptr_tail, volatile sig_atomic_t *time_to_quit){
    size_t ret_w = 0;
    int n=0;
    if((n=writen(fd, data, ptr_tail, &ret_w) == -1)){
        if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
        return _save_data(fd, ret_w, (char*)data+(ptr_tail-((ssize_t)ret_w+1)));
    }
    return 1;
}

/**
 * @param fd: file descriptor to write to
 * @param time_to_quit: variable to force to quit.
 * @param resp: this is the response to the client
 * @param is_error: 1 there was an error, 0 otherwise
 */

int _write_resp(int fd, volatile sig_atomic_t *time_to_quit, size_t resp, size_t is_error){
    size_t ret_w = 0;
    size_t to_write[2];
    to_write[0]=is_error;
    to_write[1]=resp;
    int n = 0;
    if((errno =0, n=writen(fd, to_write, sizeof(to_write), &ret_w)) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK) || *(time_to_quit) == 1) return -1;
            printf("HERE1:  %s", strerror(errno));
            global_state[fd].state = EPOLLOUT;
            global_state[fd].response = resp;
            global_state[fd].is_error = is_error;
            return -2;
    }
    return 1;
}

int open_f_l(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res = 0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    #ifdef DEBUG
    assert(path != NULL);
    #endif // DEBUG
    icl_entry_t *curr = icl_hash_find(STORE, path);
    free(path);
    if(curr == NULL || curr->O_LOCK){
        errno=EINVAL;
        return 0;
    }
    curr->OWNER = *OWNER == 0 ? CACHE->incr_entr+ID_START: *OWNER;
    *OWNER = curr->OWNER;
    return 1;
}

int open_f_c(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    errno = 0;
    icl_entry_t *curr = icl_hash_insert(STORE, (void *)path, NULL, 0, victim);
    if(curr == NULL){
        free(path);
        if(errno == EEXIST) return 0;
        else {
            print_error("Bruh I got for you some fatal errors.\nerrno: %s\n", strerror(errno));
            return -1;
        }
    }
    curr->OWNER = *OWNER == 0? CACHE->incr_entr+ID_START: *OWNER;
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck,-1);
    curr->O_LOCK = 0;
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck,-1);
    *OWNER = curr->OWNER;
    return 1;
}

int open_f_l_c (icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    errno =0;
    icl_entry_t *curr = icl_hash_insert(STORE, (void *)path, NULL, 0, victim);
    if(curr == NULL){
        free(path);
        if(errno != EEXIST) return -1;
        path = NULL;
        curr =icl_hash_find(STORE, path);
        if(curr == NULL){
            errno=ENODATA;
            return 0;
        } else if(curr->O_LOCK){
            errno=EPERM;
            return 0;
        }
        //!LOCK ACQUIRED
        LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        curr->O_LOCK=1;
        curr->open=1;
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    }
    curr->OWNER=*OWNER == 0? CACHE->incr_entr+ID_START: *OWNER;
    *OWNER = curr->OWNER;
    return 1;
}

int read_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, void **data, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1) return res;
    icl_entry_t *curr =icl_hash_find(STORE, path);
    free(path);
    if(curr ==NULL){
        errno =EINVAL;
        return 0;
    }
    if(!curr->open){
        errno=EACCES;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER != *OWNER){
        errno =EPERM;
        return 0;
    }
    if(curr->data == NULL){
        errno =ENODATA;
        return 0;
    }
    curr->am_being_used =1;
    res = _write_resp(fd, time_to_quit, curr->ptr_tail+1, IS_NOT_ERROR);
    if(res < 0) return -1;
    if(res == 0){
        return _save_data(fd, curr->ptr_tail+1, curr->data);
    }
    return _send_helper(fd, curr->data, curr->ptr_tail+1, time_to_quit);

}

int (*fun_ptr[NUM_AP])(icl_hash_t *, cach_hash_t *, int, void **, volatile sig_atomic_t *, size_t*,pointers *) ={open_f_l, open_f_c, open_f_l_c, read_f};




static void *do_magic_you_bi(void *info_w){
    Threads_w *info = (Threads_w *)info_w;
    pointers victim;
    int connfd = 0;
    int ret =0;
    size_t left_wr = 0;
    // void *to_proc_data = NULL;
    // int size_data =0;
    size_t op_OWNER[2];
    int n = 0;
    while(*(info->time_to_quit) != 1){
        memset(&victim, 0, sizeof(victim));
        while((errno = 0, n=readn(info->pipe_ready_fd[READ], &connfd, sizeof(int))) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK ) || *(info->time_to_quit) == 1) break;
            sched_yield();
        }
        if(n == 0) break;
        if(*(info->time_to_quit) == 1) break;
        if(global_state[connfd].state & EPOLLIN){
            while((n=readn(connfd, op_OWNER, sizeof(op_OWNER))) == -1){
                if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
            }
            if(*(info->time_to_quit) == 1) goto EXIT;
            ret = fun_ptr[op_OWNER[0]](info->STORE, info->CACHE, connfd, NULL, info->time_to_quit, &op_OWNER[1], &victim);
            if(ret == 1){
                _write_resp(connfd, info->time_to_quit, op_OWNER[1], IS_NOT_ERROR);
                if(victim.was_here){
                    if(victim.data){
                        ret =  _write_resp(connfd, info->time_to_quit, victim.size_data, IS_NOT_ERROR);
                        if(ret == -2){
                            _save_data(connfd, victim.size_data, victim.data);
                            goto INSERT_INTO_PIPE;
                        }
                        _send_helper(connfd, victim.data, victim.size_data, info->time_to_quit);
                    } else {
                        ret = _write_resp(connfd, info->time_to_quit, 0, IS_EMPTY);
                        if(ret == -2) goto INSERT_INTO_PIPE;
                    }
                }
            } else{
                if(ret == -2){ 
                    goto INSERT_INTO_PIPE;
                }
                _write_resp(connfd, info->time_to_quit, errno, IS_ERROR);
                if(ret == -1) goto EXIT;
            }
        } else {
            if(*(info->time_to_quit) == 1) break;
            if(global_state[connfd].response > -1){
                ret = _write_resp(connfd, info->time_to_quit, global_state[connfd].response, global_state[connfd].is_error);
                if(ret==-2) goto INSERT_INTO_PIPE;
                else if(ret == -1) goto EXIT;
                global_state[connfd].response = -1;
                global_state[connfd].state = EPOLLIN;
                global_state[connfd].sendptr = 0;
                global_state[connfd].sendbuff_end = 0;
                global_state[connfd].is_error=0;
            } 
            if(global_state[connfd].sendptr < global_state[connfd].sendbuff_end){
                if(*(info->time_to_quit) == 1) goto EXIT;
                #ifdef DEBUG
                assert(global_state[connfd].sendbuff);
                #endif // DEBUG
                size_t left = global_state[connfd].sendbuff_end - global_state[connfd].sendptr;
                ret = 0;
                if((writen(connfd, ((uint8_t*)global_state[connfd].sendbuff)+global_state[connfd].sendptr, left, &left_wr)) < 0){
                    if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
                    global_state[connfd].state = EPOLLOUT;
                    global_state[connfd].sendptr += (left-(ssize_t)left_wr);
                    goto INSERT_INTO_PIPE;
                }
                global_state[connfd].state = EPOLLIN;
                global_state[connfd].sendptr = 0;
                global_state[connfd].sendbuff_end = 0;
                global_state[connfd].sendbuff = NULL;
            }
        }
        INSERT_INTO_PIPE:
        if(victim.data) free(victim.data);
        if(victim.key) free(victim.key);
        if(*(info->time_to_quit) == 1) goto EXIT;
        while((writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), NULL)) == -1){
            if((errno != EAGAIN && errno != EWOULDBLOCK) || *(info->time_to_quit) == 1) goto EXIT;
        }
    }
    //TODO: SIGHUP HANDLE
    EXIT:
    if(victim.data) free(victim.data);
    if(victim.key) free(victim.key);
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