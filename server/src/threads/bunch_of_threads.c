#include "../../includes/bunch_of_threads.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include "../../includes/util.h"
#include <string.h>
#include "../../includes/api_sock.h"
#include "../../includes/conn.h"
#include <sys/epoll.h>
#include "../../includes/thread_for_log.h"
//REMEMBER 1 is ok, 0 the request was not ok, -1 fatal error -2 Buffer is full send request for EPOLLOUT
int _open_helper(int fd, char **path_ret, volatile sig_atomic_t *time_to_quit){
    size_t length = 0;
    int n = 0;
    char *path = NULL;
    n=readn(fd, &length, sizeof(length), time_to_quit);
    if(n < 0) return -1;
    if(n == 0){
        return 0;
    }
    assert(length > 0);
    path = (char *)calloc(length, sizeof(*path));
    if(!path){
        print_error("Houston we have a problem, we are out of memory.\nerrno:%s\n", strerror(errno));
        return -1;
    }
    n=readn(fd, path, length, time_to_quit);
    if(n ==-1)return -1;
    if(n == 0){
        return 0;
    }
    errno =0;
    path[length-1] = '\0';
    *path_ret = path;
    return 1;
}

/**
 * @param fd: file descriptor to write to
 * @param time_to_quit: variable to force to quit.
 * @param resp: this is the response to the client
 * @param is_error: 1 there was an error, 0 otherwise
 */

int _write_resp(int fd, volatile sig_atomic_t *time_to_quit, size_t resp, size_t is_error){
    size_t to_write[2];
    to_write[0]=is_error;
    to_write[1]=resp;
    int n = 0;
    n=writen(fd, to_write, sizeof(to_write), time_to_quit);
    return n;
}
/**
 * @spec: This function should be called only when the lock is acquired.
 */
int _receive_data(icl_hash_t *STORE,cach_hash_t *CACHE,int fd, icl_entry_t *curr, volatile sig_atomic_t *time_to_quit, pointers *victim, char *path){
    int n = 0;
    int ret = 0;
    size_t length = 0;
    n=readn(fd, &length, sizeof(length), time_to_quit);
    if(n ==-1) return -1;
    if(n == 0){
        return 0;
    }
    if(length > STORE->MAX_SPACE_AVAILABLE){
        errno =ENOMEM;
        return 0;
    }
    assert(length > 0);
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    assert(string_compare(curr->key, path)==1);
    //Checking if the element was evicted from memory. if yes return error
    if(curr->empty){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        errno = ENODATA;
        return 0;
    }
    //tells  the client that server is ready for data
    void*temp = realloc(curr->data, (curr->ptr_tail+length+1)*sizeof(u_int8_t));
    if(temp == NULL){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        return -1;
    }
    curr->data = temp;
    n=_write_resp(fd, time_to_quit, 0, IS_NOT_ERROR);
    if(n < 0){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        return -1;
    }
    n=readn(fd, ((u_int8_t*)curr->data)+curr->ptr_tail, length, time_to_quit);
    if(n < 0){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        return -1;
    }
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    if(n == 0){
        return 0;
    }
    curr->ptr_tail = curr->ptr_tail+(length-1);
    size_t curr_size =STORE->total_bytes +length*sizeof(u_int8_t);
    while(curr_size > STORE->MAX_SPACE_AVAILABLE && (*time_to_quit) != 1){
        curr_size-=find_victim_no_rep(CACHE, victim, time_to_quit);
        if(victim->been_modified){
            size_t len = strlen(victim->key)+1;
            ret = _write_resp(fd, time_to_quit, len, IS_NOT_ERROR);
            if(ret == -1){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return -1;
            }
            if(ret == 0){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return 0;
            }
            ret= writen(fd, victim->key, len, time_to_quit);
            if(ret == -1){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return -1;
            }
            if(ret == 0){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return 0;
            }
            ret =  _write_resp(fd, time_to_quit, victim->size_data, IS_NOT_ERROR);
            if(ret == -1){
                 if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return -1;
            }
            if(ret == 0){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return 0;
            }
            ret = writen(fd, victim->data, victim->size_data, time_to_quit);
            if(ret == -1){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return -1;
            }
            if(ret == 0){
                if(victim->data)free(victim->data);
                if(victim->key) free(victim->key);
                return 0;
            }
        }
        if(*time_to_quit == 1) return -1;
        #ifdef DEBUG
         assert(victim->data);
        assert(victim->key);
        #endif // DEBUG
        free(victim->data);
        free(victim->key);
        memset(victim, 0, sizeof(*victim));
    }
     //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&STORE->stat_lck, -1);
    STORE->total_bytes +=length*sizeof(u_int8_t);
    if(STORE->total_bytes > STORE->max_bytes) STORE->max_bytes = STORE->total_bytes;
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&STORE->stat_lck, -1);
    errno=0;
    return 0;
}



/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */
int open_f_l(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res = 0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    } 
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
/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */

int open_f_c(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
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
/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */

int open_f_l_c (icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_insert(STORE, (void *)path, NULL, 0, victim);
    if(curr == NULL){
        
        if(errno != EEXIST) return -1;
        curr =icl_hash_find(STORE, path);
        free(path);
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
/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */

int read_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    size_t len = 0, data_len =0;
    void *key = NULL, *data_file=NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
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
    curr->am_being_used+=1;
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    if(curr->key == NULL || curr->data == NULL){
        curr->am_being_used-=1;
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        errno =ENODATA;
        return -1;
    }
    res = _write_resp(fd, time_to_quit,0, IS_NOT_ERROR);
    if(res == -1) return -1;
    if(res == 0) return 0;
    len=strlen(curr->key)+1;
    data_len = curr->ptr_tail+1;
    key =calloc(len, sizeof(char));
    data_file = calloc(data_len, sizeof(u_int8_t));
    if(key != NULL && data_file != NULL){
        memcpy(key, curr->key, len);
        memcpy(data_file, curr->data, data_len);
    } else {
        if(key != NULL) free(key);
        if(data_file != NULL) free(data_file);
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        return -1;
    }
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    res = _write_resp(fd, time_to_quit, data_len, IS_NOT_ERROR);
    if(res == -1){ 
        curr->am_being_used -=1;
        free(key);
        free(data_file);
        return -1;
    }
    if(res == 0){
        curr->am_being_used -=1;
        free(key);
        free(data_file);
        return 0;
    }
    res =writen(fd, data_file, data_len, time_to_quit);
    curr->am_being_used -=1;
    dprintf(ARG_LOG_TH.pipe[WRITE], "TOTAL SENT: %zu\n", data_len);
    free(key);
    free(data_file);
    return res;
}
/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */

int read_nf(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    pointers file;
    size_t TOTAL_BYTES_SENT =0;
    size_t length = 0;
    int n =0, total = 0, res =0;
    size_t len = 0, data_len =0;
    void *key = NULL, *data_file=NULL;
    memset(&file, 0, sizeof(pointers));
    n=readn(fd, &length, sizeof(length), time_to_quit);
    if(n < 0) return -1;
    if(n == 0){
        return 0;
    }
    for(int i=0; i< STORE->nbuckets; i++){
        icl_entry_t *curr;
        for (curr=STORE->buckets[i]; curr != NULL; curr=curr->next){
            curr->am_being_used+=1;
            if(!curr->empty){
                //!LOCK ACQUIRED
                // Skipping those files that are locked by different client.
                if(curr->O_LOCK && curr->OWNER != *OWNER){
                    continue;
                }
                LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                if(curr->key == NULL || curr->data == NULL){
                    curr->am_being_used-=1;
                    //!LOCK RELEASED
                    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                    continue;
                }
                curr->ref+=2;
                len=strlen(curr->key)+1;
                dprintf(ARG_LOG_TH.pipe[WRITE], "Read File Name: %s\n", (char *) curr->key);
                data_len = curr->ptr_tail+1;
                key =calloc(len, sizeof(char));
                data_file = calloc(data_len, sizeof(u_int8_t));
                if(key != NULL && data_file != NULL){
                    memcpy(key, curr->key, len);
                    memcpy(data_file, curr->data, data_len);
                } else {
                    if(key != NULL) free(key);
                    if(data_file != NULL) free(data_file);
                    //!LOCK RELEASED
                    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                    return -1;
                }
                //!LOCK RELEASED
                UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                res= _write_resp(fd, time_to_quit, len, IS_NOT_ERROR);
                if(res == -1){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return -1;
                }
                if(res == 0){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return 0;
                }
                res =writen(fd, key, len, time_to_quit);
                if(res == -1){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return -1;
                }
                if(res == 0){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return 0;
                }
                res = _write_resp(fd, time_to_quit, data_len, IS_NOT_ERROR);
                if(res == -1){ 
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return -1;
                }
                TOTAL_BYTES_SENT += data_len;
                if(res == 0){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return 0;
                }
                res =writen(fd, data_file, data_len, time_to_quit);
                if(res == -1){ 
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return -1;
                }
                if(res == 0){
                    curr->am_being_used -=1;
                    free(key);
                    free(data_file);
                    return 0;
                }
                total++;
                free(key);
                free(data_file);
            }
            curr->am_being_used-=1;
            errno =0;
            if(length && total == length) return 0;
        }
    }
    dprintf(ARG_LOG_TH.pipe[WRITE], "TOTAL SENT: %zu\n", TOTAL_BYTES_SENT);
    errno =0;
    return 0;
}
/**
 * @param STORE: where files are stored
 * @param CACHE: pointers of files 
 * @param fd: open connection
 * @param time_to_quit: variable to let know that a signal was send
 * @param OWNER: id that lets a client access locked file.
 * @param victim: where to save victim.
 * @return 1 success, 0 request was not correct, -1 error happened.
 */
int write_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, size_t *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    if(curr == NULL){
        errno =ENODATA;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER != *OWNER){
        errno = EPERM;
        return 0;
    }
    if(*time_to_quit == 1) return -1;
    #ifdef DEBUG
    assert(string_compare(path, curr->key) == 1);
    #endif // DEBUG
    res=_write_resp(fd, time_to_quit, 0, IS_NOT_ERROR);
    if(res < 0){
        free(path);
        return res;
    }
    res = _receive_data(STORE, CACHE, fd, curr, time_to_quit, victim, path);
    free(path);
    return res;
    
}


int (*fun_ptr[NUM_AP])(icl_hash_t *, cach_hash_t *, int, volatile sig_atomic_t *, size_t*,pointers *) ={open_f_l, open_f_c, open_f_l_c, read_f, read_nf, write_f};




static void *do_magic_you_bi(void *info_w){
    Threads_w *info = (Threads_w *)info_w;
    pointers victim;
    int connfd = 0;
    int ret =0;
    // void *to_proc_data = NULL;
    // int size_data =0;
    size_t op_OWNER[2];
    int n = 0;
    while(*(info->time_to_quit) != 1){
        memset(&victim, 0, sizeof(victim));
        n=readn(info->pipe_ready_fd[READ], &connfd, sizeof(int), info->time_to_quit);
        if(n < 0) goto EXIT;
        if(n == 0){
            goto INSERT_INTO_PIPE;
        }
        if(*(info->time_to_quit) == 1) break;
        n=readn(connfd, op_OWNER, sizeof(op_OWNER), info->time_to_quit);
        if(n < 0) goto EXIT;
        if(n == 0){
            goto INSERT_INTO_PIPE;
        }
        if(*(info->time_to_quit) == 1) goto EXIT;
        ret = fun_ptr[op_OWNER[0]](info->STORE, info->CACHE, connfd, info->time_to_quit, &op_OWNER[1], &victim);
        if(ret == 1){
            ret =_write_resp(connfd, info->time_to_quit, op_OWNER[1], IS_NOT_ERROR);
            if(ret == -1){
                goto EXIT;
            }
            if(op_OWNER[0] == OPEN_F_L_C || op_OWNER[0] == OPEN_F_C){
                if(victim.data && victim.been_modified){
                    size_t len = strlen(victim.key)+1;
                    ret = _write_resp(connfd, info->time_to_quit, len, IS_NOT_ERROR);
                    if(ret == -1){
                        goto EXIT;
                    }
                    ret= writen(connfd, victim.key, len, info->time_to_quit);
                    if(ret == -1){
                        goto EXIT;
                    }
                    ret =  _write_resp(connfd, info->time_to_quit, victim.size_data, IS_NOT_ERROR);
                    if(ret == -1){
                        goto EXIT;
                    }
                    ret = writen(connfd, victim.data, victim.size_data, info->time_to_quit);
                    if(ret == -1){
                        goto EXIT;
                    }
                }
                ret = _write_resp(connfd, info->time_to_quit, 0, IS_EMPTY);
                if(ret == -1) goto EXIT;
            }
        } else{
            _write_resp(connfd, info->time_to_quit, errno, IS_ERROR);
            if(ret == -1) goto EXIT;
        }
        INSERT_INTO_PIPE:
        if(victim.data) free(victim.data);
        if(victim.key) free(victim.key);
        if(*(info->time_to_quit) == 1) goto EXIT;
        ret = writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), info->time_to_quit);
        if(ret == -1) goto EXIT;
    }
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