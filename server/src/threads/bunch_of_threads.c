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
//REMEMBER 1 is ok, 0 the request was not ok, -1 fatal error
int _open_helper(int fd, char **path_ret, volatile sig_atomic_t *time_to_quit){
    signed long long int length = 0;
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

int _write_resp(int fd, volatile sig_atomic_t *time_to_quit, signed long long int resp, signed long long int is_error){
    signed long long int to_write[2];
    to_write[0]=is_error;
    to_write[1]=resp;
    return writen(fd, to_write, sizeof(to_write), time_to_quit);
}
/**
 * @spec: This function should be called only when the lock is acquired.
 */
int _receive_data(icl_hash_t *STORE,cach_hash_t *CACHE,int fd, icl_entry_t *curr, volatile sig_atomic_t *time_to_quit, pointers *victim, signed long long int *OWNER){
    int n = 0;
    int ret = 0;
    signed long long int length = 0;
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
    LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
    //Checking if the element was evicted from memory. if yes return error
    if(curr->empty || !curr->open){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        errno = ENODATA;
        return 0;
    }
    //tells  the client that server is ready for data
    void*temp = realloc(curr->data, (curr->ptr_tail+length)*sizeof(u_int8_t*));
    if(temp == NULL){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        return -1;
    }
    curr->data = temp;
    n=_write_resp(fd, time_to_quit, *OWNER, IS_NOT_ERROR);
    if(n < 0){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        return -1;
    }
    
    n=readn(fd, ((u_int8_t*)curr->data+curr->ptr_tail), length, time_to_quit);
    if(n < 0){
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        return -1;
    }
    if( curr->ptr_tail>0) curr->been_modified =1;
    curr->ptr_tail = curr->ptr_tail+(length-1) > 0? curr->ptr_tail+(length-1): 0;
    signed long long int curr_size =STORE->total_bytes +length*sizeof(u_int8_t);

    dprintf(ARG_LOG_TH.pipe[WRITE], "WRITE: %s\n", (char*) curr->key);
    dprintf(ARG_LOG_TH.pipe[WRITE], "BYTES: %lld\n",  length);
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
    if(n == 0){
        return 0;
    }
    while(curr_size > STORE->MAX_SPACE_AVAILABLE && (*time_to_quit) != 1){
        //finds and evicts a victim from cache
        curr_size-=find_victim_no_rep(CACHE, victim, time_to_quit);
        //if victim was modified e.i got appended that it sends back to the client
        if(victim->been_modified){
            signed long long int len = strlen(victim->key)+1;
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
        if(victim->data == NULL){
            if(victim->key){
                memset(victim, 0, sizeof(*victim));
                free(victim->key);
            }
            continue;

        }
        free(victim->data);
        free(victim->key);
        memset(victim, 0, sizeof(*victim));
        if(*time_to_quit == 1) return -1;
    }
     //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&(STORE->stat_lck), -1);
    STORE->total_bytes +=length*sizeof(u_int8_t);
    if(STORE->total_bytes > STORE->max_bytes) STORE->max_bytes = STORE->total_bytes;
    dprintf(ARG_LOG_TH.pipe[WRITE], "MAX_BYTES: %lld\n", STORE->max_bytes);
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&(STORE->stat_lck), -1);
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
int open_f_l(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
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
    CACHE->incr_entr++;
    free(path);
    if(curr == NULL){
        errno=EINVAL;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER != *OWNER){
        errno=EPERM;
        return 0;
    }
    if(curr->O_LOCK) return 1;
    LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        curr->O_LOCK=1;
        curr->open=1;
        //!LOCK RELEASED
    dprintf(ARG_LOG_TH.pipe[WRITE], "OPEN: %s.\n", (char*) curr->key);
    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
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

int open_f_c(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno = 0;
    icl_entry_t *curr = icl_hash_insert(STORE, (void *)path, NULL, victim);
    if(curr == NULL){
        free(path);
        if(errno == EEXIST) return 0;
        else {
            print_error("got for you some fatal errors.\nerrno: %s\n", strerror(errno));
            return -1;
        }
    }
    curr->OWNER = *OWNER == 0? CACHE->incr_entr+ID_START: *OWNER;
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck),-1);
    curr->O_LOCK = 0;
    dprintf(ARG_LOG_TH.pipe[WRITE], "OPEN: %s\n", (char*) curr->key);
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck),-1);
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

int open_f_l_c (icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    icl_entry_t *curr = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    curr = icl_hash_insert(STORE, (void *)path, NULL, victim);
    if(curr == NULL){
        if(errno != EEXIST) return -1;
        curr =icl_hash_find(STORE, path);
        free(path);
        if(curr == NULL || curr->key == NULL){
            errno=ENODATA;
            return 0;
        } else if(curr->O_LOCK && curr->OWNER != *OWNER){
            errno=EPERM;
            return 0;
        }
        //!LOCK ACQUIRED
        LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        if(!curr->empty ){
            curr->O_LOCK=1;
            curr->open=1;
        } else {
             //!LOCK RELEASED
            UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
            errno=ENODATA;
            return 0;
        }
        dprintf(ARG_LOG_TH.pipe[WRITE], "OPEN: %s\n", (char*) curr->key);
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        curr->OWNER=*OWNER == 0? CACHE->incr_entr+ID_START: *OWNER;
        *OWNER = curr->OWNER;
        return 1;
    }
     //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
    dprintf(ARG_LOG_TH.pipe[WRITE], "OPEN: %s\n", (char*) curr->key);
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
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

int read_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    signed long long int len = 0, data_len =0;
    void *key = NULL, *data_file=NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    CACHE->incr_entr++;
    icl_entry_t *curr =icl_hash_find(STORE, path);
    free(path);
    if(curr ==NULL){
        errno =EINVAL;
        return 0;
    }
    if(!curr->open){
        errno=ENOENT;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER != *OWNER){
        errno =EPERM;
        return 0;
    }
    curr->am_being_used+=1;
    //!LOCK ACQUIRED
    LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
    if(curr->key == NULL || curr->data == NULL || !curr->open){
        curr->am_being_used-=1;
        //!LOCK RELEASED
        UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
        errno =ENODATA;
        return 0;
    }
    res = _write_resp(fd, time_to_quit,*OWNER, IS_NOT_ERROR);
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
        curr->am_being_used-=1;
        return -1;
    }
    dprintf(ARG_LOG_TH.pipe[WRITE], "READ: %s\n", (char*) curr->key);
    dprintf(ARG_LOG_TH.pipe[WRITE], "SENT: %lld\n", data_len);
    //!LOCK RELEASED
    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
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

int read_nf(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    pointers file;
    signed long long int length = 0;
    int n =0, total = 0, res =0;
    signed long long int len = 0, data_len =0;
    CACHE->incr_entr++;
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
                // Skipping those files that are locked by different client or closed files.
                if((curr->O_LOCK && curr->OWNER != *OWNER) || !curr->open){
                    curr->am_being_used-=1;
                    continue;
                }
                LOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
                //Rechecking the condition
                if(curr->empty || !curr->open){
                    curr->am_being_used-=1;
                    //!LOCK RELEASED
                    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
                    continue;
                }
                if(curr->key == NULL || curr->data == NULL){
                    curr->am_being_used-=1;
                    //!LOCK RELEASED
                    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
                    continue;
                }
                curr->ref+=2;
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
                    UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
                    curr->am_being_used -=1;
                    return -1;
                }
                //!LOCK RELEASED
                UNLOCK_IFN_RETURN(&(curr->wr_dl_ap_lck), -1);
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
                dprintf(ARG_LOG_TH.pipe[WRITE], "READ: %s\n", (char*) key);
                dprintf(ARG_LOG_TH.pipe[WRITE], "SENT: %lld\n", data_len);
                total++;
                free(key);
                free(data_file);
            }
            curr->am_being_used-=1;
            errno =0;
            if(length && total == length) return 0;
        }
    }
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
int write_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    CACHE->incr_entr++;
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    if(curr == NULL){
        free(path);
        errno =ENODATA;
        return 0;
    }
    if(!curr->open){
        free(path);
        errno =ENOENT;
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
    res=_write_resp(fd, time_to_quit, *OWNER, IS_NOT_ERROR);
    if(res < 0){
        free(path);
        return res;
    }
    res = _receive_data(STORE, CACHE, fd, curr, time_to_quit, victim, OWNER);
    free(path);
    return res;
    
}

int lock_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    int resp = 0;
    char data;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    CACHE->incr_entr++;
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    if(curr == NULL){
        free(path);
        errno =ENODATA;
        return 0;
    }
    free(path);
    if(!curr->open){
        errno=ENOENT;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER == *OWNER){
        if(curr->empty) {
            errno=ENODATA;
            return 0;
        }
        dprintf(ARG_LOG_TH.pipe[WRITE], "LOCK: %s\n", (char*) curr->key);
        return 1;
    }
    while(!(*time_to_quit)){
        //lock file or wait
        if(curr->O_LOCK == 0){
            if(!TRYLOCK(&curr->wr_dl_ap_lck)){
                //we need to check again. 
                if(curr->O_LOCK == 1){
                    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                    continue;
                }
                dprintf(ARG_LOG_TH.pipe[WRITE], "LOCK: %s\n", (char*) curr->key);
                curr->O_LOCK=1;
                curr->OWNER=*OWNER;
                UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
                return 1;
            }
        }
        if(curr->empty) {
            errno=ECANCELED;
            return 0;
        }
        resp = recv(fd,&data,1, MSG_PEEK);
        if(resp == 0){
            errno=EOWNERDEAD;
            return 0;
        }
        sched_yield();
    }
    errno=EHOSTDOWN;
    return 0;
}

int unlock_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    CACHE->incr_entr++;
    if(curr == NULL || curr->empty){
        free(path);
        errno =ENODATA;
        return 0;
    }
    free(path);
    if(!curr->open){
        errno=ENOENT;
        return 0;
    }
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    if(curr->O_LOCK && curr->OWNER == *OWNER){
        curr->O_LOCK = 0;
        dprintf(ARG_LOG_TH.pipe[WRITE], "UNLOCK: %s\n", (char*) curr->key);
        UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
        return 1;
    }
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    errno=EPERM;
    return 0;
}

int close_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    CACHE->incr_entr++;
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    if(curr == NULL){
        free(path);
        errno =ENODATA;
        return 0;
    }
    free(path);
    if(!curr->open){
        errno=ENOENT;
        return 0;
    }
    if(curr->O_LOCK && curr->OWNER != *OWNER){
        errno=EPERM;
        return 0;
    }
    LOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    curr->open = 0;
    curr->O_LOCK=0;
    UNLOCK_IFN_RETURN(&curr->wr_dl_ap_lck, -1);
    dprintf(ARG_LOG_TH.pipe[WRITE], "CLOSE: %s\n", (char*) curr->key);
    return 1;
}

int remove_f(icl_hash_t *STORE, cach_hash_t *CACHE, int fd, volatile sig_atomic_t *time_to_quit, signed long long int *OWNER, pointers *victim){
    int res =0;
    pointers vict;
    memset(&vict, 0, sizeof(vict));
    char *path = NULL;
    res = _open_helper(fd, &path, time_to_quit);
    CACHE->incr_entr++;
    if(res < 1){
        if(path) free(path);
        return res;
    }
    errno =0;
    icl_entry_t *curr = icl_hash_find(STORE, path);
    if(curr == NULL){
        free(path);
        errno =ENODATA;
        return 0;
    }
    res=icl_hash_delete_ext(STORE, path, &vict, *OWNER, time_to_quit);
    free(path);
    if(vict.data) free(vict.data);
    if(vict.key) free(vict.key);
    memset(&vict, 0, sizeof(vict));
    return res;
}

int (*fun_ptr[NUM_AP])(icl_hash_t *, cach_hash_t *, int, volatile sig_atomic_t *, signed long long int*,pointers *) ={open_f_l, \
 open_f_c, open_f_l_c, read_f, read_nf, write_f, lock_f, unlock_f, close_f, remove_f};



/**
 * @brief: each thread waits for the fd to serve. Each API function has a place 
 * in the array of function, so when a client wants something it has to send the correct index,
 * if clients send an incorrect index than server will respond with ECANCELED
 * @param info_w: has all settings needed to delivery the result. Two pipes to read from and write to ready/served fd.
 * time_to_quit tells all worker that the time has come to exit.
 * STORE is where all files with data are saved, CACHE is where all pointers are placed to hopefully have an efficient cache.
 * num_thr: total threads to be created
 * created_thr created threads.
 */
static void *do_magic(void *info_w){
    Threads_w *info = (Threads_w *)info_w;
    pointers victim;
    memset(&victim, 0, sizeof(victim));
    int connfd = 0;
    int ret =0;
    // void *to_proc_data = NULL;
    // int size_data =0;
    signed long long int op_OWNER[2] ={0};
    while(*(info->time_to_quit) != 1){
        //!LOCK ACQUIRED
        LOCK_IFN_GOTO(&(info->LOCK_FD), EXIT);
        while(info->tail_fds <= 0 && *(info->time_to_quit) != 1){
            pthread_cond_wait(&(info->cond), &(info->LOCK_FD));
        }
         if(*(info->time_to_quit) == 1){
            //!LOCK RELEASED
            UNLOCK_IFN_GOTO(&(info->LOCK_FD), EXIT);
            goto EXIT;
        }
        connfd = info->fds[--info->tail_fds];
        UNLOCK_IFN_GOTO(&(info->LOCK_FD), EXIT);
        while((errno =0, readn_return(connfd, op_OWNER, sizeof(op_OWNER), info->time_to_quit))>0){
        dprintf(ARG_LOG_TH.pipe[WRITE], "THREAD: %lu FD: %d TIME: %ld\n", pthread_self(), connfd, time(NULL));
            if(*(info->time_to_quit) == 1) goto EXIT;
            if(op_OWNER[0] > NUM_AP || op_OWNER[1] > (info->CACHE->incr_entr+ID_START+2)){
                errno=ECANCELED;
                ret = _write_resp(connfd, info->time_to_quit, errno, IS_ERROR);
                    if(ret == -1){
                    goto EXIT;
                }
                goto INSERT_INTO_PIPE;
            }
            ret = fun_ptr[op_OWNER[0]](info->STORE, info->CACHE, connfd, info->time_to_quit, &op_OWNER[1], &victim);
            if(ret == 1){
                ret =_write_resp(connfd, info->time_to_quit, op_OWNER[1], IS_NOT_ERROR);
                if(ret == -1){
                    goto EXIT;
                }
                if(ret==0) goto INSERT_INTO_PIPE;
                if(op_OWNER[0] == OPEN_F_L_C || op_OWNER[0] == OPEN_F_C){
                    if(victim.data && victim.been_modified){
                        assert(victim.size_data);
                        signed long long int len = strlen(victim.key)+1;
                        ret = _write_resp(connfd, info->time_to_quit, len, IS_NOT_ERROR);
                        if(ret == -1){
                            goto EXIT;
                        }
                        if(ret==0) goto INSERT_INTO_PIPE;
                        ret= writen(connfd, victim.key, len, info->time_to_quit);
                        if(ret == -1){
                            goto EXIT;
                        }
                        if(ret==0) goto INSERT_INTO_PIPE;
                        ret =  _write_resp(connfd, info->time_to_quit, victim.size_data, IS_NOT_ERROR);
                        if(ret == -1){
                            goto EXIT;
                        }
                        if(ret==0) goto INSERT_INTO_PIPE;
                        ret = writen(connfd, victim.data, victim.size_data, info->time_to_quit);
                        if(ret == -1){
                            goto EXIT;
                        }
                        if(ret==0) goto INSERT_INTO_PIPE;
                    }
                    ret = _write_resp(connfd, info->time_to_quit, 0, IS_EMPTY);
                    if(ret==0) goto INSERT_INTO_PIPE;
                    if(ret == -1){ 
                        goto EXIT;
                    }
                    if(*(info->time_to_quit) == 1) goto EXIT;
                    if(victim.data)
                        free(victim.data);
                    if(victim.key)
                        free(victim.key);
                    memset(&victim, 0 , sizeof(victim));
                }
            } else{
                 if(errno == SIGPIPE && ret==0) goto INSERT_INTO_PIPE;
                _write_resp(connfd, info->time_to_quit, errno, IS_ERROR);
                if(ret == -1) goto EXIT;
            }
        }
        INSERT_INTO_PIPE:
        if(errno == SIGPIPE){
            close(connfd);
            continue;
        }
        if(victim.data){ 
            free(victim.data);
            memset(&victim, 0 , sizeof(victim));
            }
        if(victim.key){ 
            free(victim.key);
            memset(&victim, 0 , sizeof(victim));
        }
        
        ret = writen(info->pipe_done_fd[WRITE], &connfd, sizeof(connfd), info->time_to_quit);
        if(ret == -1){ 
            goto EXIT;
        }
    }
    EXIT:
    *info->time_to_quit = 1;
    if(victim.data) free(victim.data);
    if(victim.key) free(victim.key);
    close(connfd);
    return NULL;
}
/**
 * 
 * @param info: all variables used to help the execution of do_magic_you_bi
 */
int kill_them(Threads_w *info){
    if(info == NULL){
        errno = EINVAL;
        return -1;
    }
    BCAST_RETURN(&(info->cond), -1);
    for(signed long long int i = 0; i < info->created_thr; i++){
        if(pthread_join(info->my_thr[i], NULL) != 0){
            errno = EFAULT;
            return -1;
        }
    }
    close(info->pipe_done_fd[READ]);
    close(info->pipe_done_fd[WRITE]);
    pthread_cond_destroy(&(info->cond));
    pthread_mutex_destroy(&info->LOCK_FD);
    free(info->fds);
    free(info->my_thr);
    return 0;
}
/**
 * @param info: all variables to be used in the process of serving clients.
 */
int create_them (Threads_w *info){
    if(info->num_thr < 0){
        errno = EINVAL;
        return -1;
    }
    info->created_thr = 0;
    info->my_thr = (pthread_t*)malloc(sizeof(pthread_t)*info->num_thr);
    info->fds = (int*)calloc(SENDBUFF_SIZE, sizeof(int));
    info->size_fds=SENDBUFF_SIZE;
    info->tail_fds =0;
    if(info->my_thr ==NULL || info->fds == NULL){
        fprintf(stderr, "no more memory.\n");
        return -1;
    }
    if((pthread_mutex_init(&(info->LOCK_FD), NULL) != 0) || (pthread_cond_init(&(info->cond), NULL) != 0)){
        free(info->my_thr);
        free(info->fds);
        return -1;
    }
    for(signed long long int i = 0; i < info->num_thr; i++){
        if(pthread_create(&(info->my_thr[i]), NULL, do_magic, (void*)info) != 0){
            kill_them(info);
            errno = EFAULT;
            return -1;
        }
        info->created_thr++;
    }
    return 0;
}