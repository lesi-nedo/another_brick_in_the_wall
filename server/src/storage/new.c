#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "../../includes/new.h"
#include "../../includes/util.h"
#include <pthread.h>
#include <limits.h>
#include "../../includes/thread_for_log.h"

#define SOME_CONST 50 //how many times each level gets scan before stop and report a bug

unsigned int get_next_index(unsigned int key, int NUM_GROUP){

    return key % NUM_GROUP;
}

/**
 * Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 *
 * @returns pointer to new hash table.
 */

cach_hash_t *
cach_hash_create(struct Cache_settings sett, unsigned int (*hash_function)(unsigned int, int) )
{
    cach_hash_t *ht;
    int i;
    int res= 0;
    size_t pos_row =0;
    int extr = 0;

    ht = (cach_hash_t*) malloc(sizeof(cach_hash_t));
    if(!ht) return NULL;

    ht->incr_entr = 0;
    ht->buckets = (cach_fd_t**)malloc(sett.NUM_GROUP * sizeof(cach_fd_t*));
    for(size_t i=0; i<sett.NUM_GROUP; i++){ 
        ht->buckets[i] =(cach_fd_t*)malloc(sizeof(cach_fd_t));
        if(ht->buckets[i] == NULL){
            for(size_t ind = 0; ind < i; i++){
                free(ht->buckets[ind]);
            }
            free(ht);
            fprintf(stderr, "Get some more memory and then come back.\n");
            return NULL;
        }
    }
    if(!ht->buckets) return NULL;
    ht->nbuckets = sett.NUM_GROUP;
    ht->NUM_GROUP = sett.NUM_GROUP;
    ht->CACHE_RANGE = sett.CACHE_RANGE;
    ht->MAX_LAST_LEVEL = sett.NUM_GROUP*sett.CACHE_RANGE;
    ht->nfiles = 0;
    ht->START_INI_CACHE = sett.START_INI_CACHE > sett.NUM_GROUP? sett.START_INI_CACHE: sett.NUM_GROUP; ;
    ht->MAX_SPACE_AVAILABLE = sett.SPACE_AVAILABLE;
    extr = (int) (sett.START_INI_CACHE*sett.EXTRA_CACHE_SPACE);;
    for(i=0;i<ht->NUM_GROUP;i++){
        ht->buckets[i]->group = i;
        ht->buckets[i]->head = NULL;
        ht->buckets[i]->tail = NULL;
        ht->buckets[i]->nfiles_row = 0;
        ht->buckets[i]->num_dead = 0;
        ht->buckets[i]->threads_in = 0;
        if((res =pthread_mutex_init(&(ht->buckets[i]->mutex_for_cleanup), NULL)) != 0){
            for(size_t ind = 0; ind < i; i++){
                free(ht->buckets[ind]);
            }
            free(ht);
            fprintf(stderr, "Au revoir. ERROR: %s\n", strerror(res));
            return NULL;
        }
        ht->buckets[i]->min_and_you_in = sett.CACHE_RANGE*i;
        ht->buckets[i]->max_and_you_out = sett.CACHE_RANGE*(i+1);
    }
    for(size_t i = 0; i < ht->START_INI_CACHE+extr; i++){
        if(i == ht->NUM_GROUP*pos_row) pos_row++;
        unsigned int hashed = i % ht->NUM_GROUP;
        cach_entry_t *to_ins = (cach_entry_t *)malloc(sizeof(cach_entry_t));
        if(to_ins == NULL){
            cach_hash_destroy(ht);
            fprintf(stderr, "Jeez, bro get some extra memory cause we ain't got no more of that.\n");
            return NULL;
        }
        to_ins->am_dead = 1;
        to_ins->ref = NULL;
        to_ins->group = hashed;
        to_ins->me_but_in_store = NULL;
        to_ins->file_name = NULL;
        to_ins->time = 0;
        if((res=pthread_mutex_init(&to_ins->mutex, NULL)) != 0){
            cach_hash_destroy(ht);
            fprintf(stderr, "Wasn't my fault, however I'm sorry <3 <3. ERROR: %s\n", strerror(res));
            free(to_ins);
            return NULL;
        }
        to_ins->next = NULL;
        to_ins->prev = NULL;
        if(ht->buckets[hashed]->head == NULL){ 
            ht->buckets[hashed]->head = to_ins;
            ht->buckets[hashed]->tail = to_ins;
        } else {
            to_ins->prev = ht->buckets[hashed]->tail;
            ht->buckets[hashed]->tail->next = to_ins;
            ht->buckets[hashed]->tail = to_ins;
        }
        ht->buckets[hashed]->num_dead = pos_row;
    }

    ht->hash_function = hash_function ? hash_function : get_next_index;
    return ht;
}
/**
 * @param -- takes pid of the calling thread
 */
unsigned long
get_seed(pthread_t th_pid){
    th_pid += time(0);
    th_pid -=clock();
    th_pid ^= (unsigned long) &th_pid;
    return th_pid;
}

/**
 * @param -- takes no parameters 
 * @return -- return the difference between two random number
 */
signed int
get_rand_diff(){
    long res1= 0, res2 = 0; 
    struct drand48_data buff;
    unsigned long seed = get_seed(pthread_self());
    srand48_r(seed, &buff);
    lrand48_r(&buff, &res1);
    lrand48_r(&buff, &res2);
    return res1 -res2;
}

/**
 * Uses a reentrant type of function to generate random numbers
 * @return -- return a pseudo random number between [0,2^31]
 */
unsigned long int
get_rand(){
    long res1= 0; 
    struct drand48_data buff;
    unsigned long seed = get_seed(pthread_self()^getpid());
    srand48_r(seed, &buff);
    lrand48_r(&buff, &res1);
    return res1;
}

/**
 * @param val -- represents the number of time a file was written, read, open
 * @return  -- new index to be used calculated from val
 */
int 
_get_level(int val){
    int res = val;
    int index = 0;
    if(val >= MY_CACHE->MAX_LAST_LEVEL){
        return MY_CACHE->NUM_GROUP -1;
    }
    if(val <=0) return 0;
    while((res % MY_CACHE->CACHE_RANGE) == 0) res--;
    index = res / MY_CACHE->CACHE_RANGE;
    int max = MY_CACHE->buckets[index]->max_and_you_out;
    if(val >= max){
        index = index +1 >= MY_CACHE->NUM_GROUP? MY_CACHE->NUM_GROUP -1: index+1;
        return index;
    }
    #ifdef DEBUG
        assert(index >= 0);
    #endif // DEBUG
    return index;
}

/**
 * @param curr -- starting point of the cache
 * @param file_in -- holds the file name to be insert into the cache
 * @param tail_or_head -- 0 start from head, 1 from tail_or_head
 * This function helps the caller to find an empty spot in cache
 */

cach_entry_t
*_till_end(cach_entry_t *curr, icl_entry_t *file_in, int tail_or_head, int index){
    MY_CACHE->buckets[index]->threads_in++;
    while(curr){
        if(curr->am_dead){
            if(TRYLOCK(&curr->mutex) == 0){
                //!LOCK ACQUIRED
                if(curr->am_dead){
                    LOCK(&file_in->wr_dl_ap_lck)
                    if(!file_in->empty){
                        curr->am_dead = 0;
                        curr->me_but_in_store= file_in;
                        MY_CACHE->nfiles++;
                        curr->ref = &file_in->ref;
                        curr->time = file_in->time;
                        curr->file_name = file_in->key;
                        file_in->me_but_in_cache = curr;
                        MY_CACHE->buckets[curr->group]->nfiles_row++;
                        MY_CACHE->buckets[curr->group]->num_dead--;
                        signed int diff = MY_CACHE->incr_entr -  MY_CACHE->buckets[curr->group]->min_and_you_in;
                        file_in->ref = diff <= ~diff+1 ? 0 : diff;
                        MY_CACHE->buckets[index]->threads_in--;
                        //!LOCK RELEASED if am_dead == 1
                        UNLOCK(&curr->mutex);
                        UNLOCK(&file_in->wr_dl_ap_lck);
                        return curr;
                    } else {
                        MY_CACHE->buckets[index]->threads_in--;
                        UNLOCK(&curr->mutex);
                        UNLOCK(&file_in->wr_dl_ap_lck);
                        errno = ENODATA;
                        return NULL;
                    }
                }
                 //!LOCK RELEASED if am_dead == 1
                UNLOCK(&curr->mutex);
            }
        }
        if(tail_or_head){
            curr = curr->prev;
        } else curr = curr->next;
    }
    MY_CACHE->buckets[index]->threads_in--;
    return NULL;
}
/**
 * @param ht -- cache to work with
 * @param index -- starting point, must be 0 <= index > my-cache->NUM_GROUP
 * @return -- new index where was a free block
 */

int _find_dead_helper(cach_hash_t *ht, int index){
    index = (get_rand() ^ pthread_self()) % ht->NUM_GROUP;
    //0 to decrement 1 to increment
    int min_or_plu = 0;
    int limit = 0;
    while(limit<SOME_CONST){
        if((ht->buckets[index]->num_dead)- (ht->buckets[index]->threads_in-1) > 0){
            return index;
        }
        if(!min_or_plu){
            index--;
        } else index++;
        if(index < 0){
            index = 0;
            min_or_plu = 1;
            sched_yield();
            limit++;
        }
        if(index >= MY_CACHE->NUM_GROUP){
            index = MY_CACHE->NUM_GROUP-1;
            min_or_plu = 0;
            sched_yield();
            limit++;
        }
    }
    return -1;
}
/**
 * @brief: I almost went nuts because of this function.
My idea is each threads start from position one, variable temp holds the file to be insert, check the first row for empty spots but if there are already 
threads looking and empty rows are less than the number of threads currently presents in the level than it passes one level below. If it finds an empty spots tries to get the lock for writing the pointer in cache, and waits 
for the lock associated at each file in store to update pointers. So if a thread finds a pointer that should not be there because it was referenced more than the level has the range or less than it tries to get the lock and 
replaces with the fresh one, the "victim" will be placed in the correct position but if the level is full in which it belongs than the thread tries to find the closest to the correct level.
It might happened that a thread holds a variable already dead than it releases the locks and frees the memory.
I can guarantee(at least I hope so) that the values in cache and in files are correct, i.e each pointers are correct (tested with 100 threads running for "long time"), and there are no deadlocks relative to the part of cache and store.
 * @param curr -- starting point from which will be searched
 * @param temp -- temporary entries to hold all variables information to be insert into cache
 * @return -- 0 success
 */
int
 _helper_ins_rand(cach_hash_t *ht, cach_entry_t *temp, icl_entry_t *file_in_store, int index, pointers *ret){
    //0 starts from head 1  from tail.
    int tail_or_head = get_rand_diff() < 0 ? 1 : 0;
    MY_CACHE->buckets[index]->threads_in++;
    int ref = 0;
    icl_entry_t *file_in = file_in_store;
    cach_entry_t *curr = tail_or_head == 0? ht->buckets[index]->head: ht->buckets[index]->tail;
    if(ht == NULL || temp == NULL || file_in == NULL){
        fprintf(stderr, "\033[1;31mFatal ERROR\033[1;37m in _helper_ins_rand, one of the argument is null, congratulation your code has a bug.\033[0;37m\n");
        MY_CACHE->buckets[index]->threads_in--;
        return -1;
    }
    while(curr != NULL){
        if(curr->am_dead){
            //JUMPS TO THE END BECAUSE WE NEED TO DELETE TEMP
            if(TRYLOCK(&curr->mutex) == 0){
                //!LOCK ACQUIRED
                if(curr->am_dead){
                    pthread_mutex_t *UNLOCK_ME = &file_in->wr_dl_ap_lck;
                    LOCK(UNLOCK_ME);
                    if(!file_in->empty && string_compare(file_in->key, temp->file_name) && temp->time == file_in->time){
                        curr->am_dead = 0;
                        MY_CACHE->nfiles++;
                        curr->ref = temp->ref;
                        curr->me_but_in_store= file_in;
                        curr->file_name = temp->file_name;
                        curr->time = temp->time;
                        file_in->me_but_in_cache = curr;
                        ht->buckets[index]->nfiles_row++;
                        ht->buckets[index]->num_dead--;
                        //!LOCK RELEASED if am_dead == 1
                        MY_CACHE->buckets[index]->threads_in--;
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    } else {
                        // printf("TIME: %ld------%ld FILES: %s ----- %s\n", temp->time, file_in->time, (char*)file_in->key, temp->file_name);
                        MY_CACHE->buckets[index]->threads_in--;
                        //! LOCKS RELEASED
                        UNLOCK(UNLOCK_ME);
                        UNLOCK(&curr->mutex);
                        free(temp);
                        return 0;
                    }
                }   //!LOCK RELEASED if am_dead == 0
                else {
                    UNLOCK(&curr->mutex);
                }
            }
            if(tail_or_head){
                curr = curr->prev;
            } else curr = curr->next;
                continue;

        } else if(curr->am_dead || (ref = curr->ref != NULL ? *curr->ref : MY_CACHE->incr_entr, (ht->incr_entr - ref)) >= ht->buckets[index]->max_and_you_out){
            if(TRYLOCK(&curr->mutex) == 0){
                //!LOCK ACQUIRED
                //rechecking the condition
                    //rechecking the first condition
                if(curr->am_dead){
                    pthread_mutex_t *UNLOCK_ME = &file_in->wr_dl_ap_lck;
                    //!SECOND LOCK ACQUIRED
                    LOCK(UNLOCK_ME);
                    if(!file_in->empty && string_compare(file_in->key, temp->file_name) && temp->time == file_in->time){
                        curr->am_dead = 0;
                        MY_CACHE->nfiles = MY_CACHE->nfiles +1;
                        ht->buckets[index]->nfiles_row++;
                        ht->buckets[index]->num_dead--;
                        curr->ref = temp->ref;
                        curr->me_but_in_store= file_in;
                        curr->file_name = temp->file_name;
                        curr->time = temp->time;
                        file_in->me_but_in_cache = curr;
                        MY_CACHE->buckets[index]->threads_in--;
                        //!LOCKS RELEASED if am_dead == 1
                        UNLOCK(UNLOCK_ME);
                        UNLOCK(&curr->mutex);
                        free(temp);
                        return 0;
                    } else {
                        MY_CACHE->buckets[index]->threads_in--;
                        //!LOCKS RELEASED
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    }
                } else if ((ht->incr_entr - *(curr->ref)) >= ht->buckets[index]->max_and_you_out) {
                    char *temp_fl = NULL;
                    icl_entry_t *temp_me = NULL;
                    time_t temp_t = 0;
                    pthread_mutex_t *UNLOCK_ME = &file_in->wr_dl_ap_lck;
                    //!LOCK ACQUIRED
                    LOCK(UNLOCK_ME);
                    if(!file_in->empty && string_compare(file_in->key, temp->file_name) && temp->time == file_in->time){
                        temp_fl = curr->file_name;
                        temp_me = curr->me_but_in_store;
                        temp_t = curr->time;

                        curr->ref = temp->ref;
                        curr->am_dead = 0;
                        curr->me_but_in_store= file_in;
                        curr->file_name = temp->file_name;
                        file_in->me_but_in_cache = curr;
                        curr->time = temp->time;

                        temp->file_name = temp_fl;
                        temp->me_but_in_store = temp_me;
                        temp->ref = &temp_me->ref;
                        temp_me->me_but_in_cache = NULL;
                        temp->time = temp_t;
                        file_in =temp_me;
                        //!LOCK RELEASED if am_dead == 0 and new_ind > max_out
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                    } else {
                        MY_CACHE->buckets[index]->threads_in--;
                        // printf("TIME: %ld------%ld FILES: %s ----- %s\n", temp->time, file_in->time, (char*)file_in->key, temp->file_name);
                        //!LOCK RELEASED
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    }
                    int old_ind = index;
                    index = _get_level(ht->incr_entr - ref);
                    MY_CACHE->buckets[old_ind]->threads_in--;
                    MY_CACHE->buckets[index]->threads_in++;
                    #ifdef DEBUG
                        assert(file_in != NULL);
                    #endif // DEBUG
                    if(old_ind == index && (ht->buckets[index]->num_dead - (ht->buckets[index]->threads_in-1)) <= 0){
                        break;
                    }
                    if(old_ind == index && (ht->buckets[index]->num_dead - (ht->buckets[index]->threads_in-1)) > 0){
                        if(tail_or_head){
                            curr=curr->prev;
                        } else curr = curr->next;
                        continue;
                    }
                    if(get_rand_diff() < 0){
                        tail_or_head = 1;
                        curr = ht->buckets[index]->tail;
                        #ifdef DEBUG
                        assert(curr != NULL);
                        #endif // DEBUG
                        continue;
                    } else {
                        tail_or_head = 0;
                        curr = ht->buckets[index]->head;
                        #ifdef DEBUG
                        assert(curr != NULL);
                        #endif // DEBUG
                        continue;
                    }
                    //!LOCK RELEASED if < max_and_you_out
                } else{
                    UNLOCK(&curr->mutex);
                }
            }
            if(tail_or_head){
                curr = curr->prev;
            } else curr = curr->next;
            continue;
        }else if( curr->am_dead || (ref = curr->ref != NULL ? *curr->ref : MY_CACHE->incr_entr, (ht->incr_entr - ref)) < ht->buckets[index]->min_and_you_in) {
            if(TRYLOCK(&curr->mutex)==0){
                //!LOCK ACQUIRED
                //rechecking the condition
                 //rechecking the first condition
                if(curr->am_dead){
                    pthread_mutex_t *UNLOCK_ME = &file_in->wr_dl_ap_lck;
                    LOCK(UNLOCK_ME);
                    if(!file_in->empty && string_compare(file_in->key, temp->file_name) && temp->time == file_in->time){
                        curr->am_dead = 0;
                        MY_CACHE->nfiles = MY_CACHE->nfiles +1;
                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        curr->me_but_in_store= file_in;
                        curr->time = temp->time;
                        file_in->me_but_in_cache = curr;
                        ht->buckets[index]->nfiles_row++;
                        ht->buckets[index]->num_dead--;
                        //!LOCK RELEASED if am_dead == 1
                        MY_CACHE->buckets[index]->threads_in--;
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    } else {
                        // printf("TIME: %ld------%ld FILES: %s ----- %s\n", temp->time, file_in->time, (char*)file_in->key, temp->file_name);
                        MY_CACHE->buckets[index]->threads_in--;
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    }
                } else if((ht->incr_entr - *(curr->ref)) < ht->buckets[index]->min_and_you_in){
                    void *temp_fl = NULL;
                    icl_entry_t *temp_me = NULL;
                    time_t temp_t = 0;
                    pthread_mutex_t *UNLOCK_ME = &file_in->wr_dl_ap_lck;
                    //!LOCK ACQUIRED
                    LOCK(UNLOCK_ME);
                    if(!file_in->empty && string_compare(file_in->key, temp->file_name) && temp->time == file_in->time){
                        temp_fl = curr->file_name;
                        temp_me = curr->me_but_in_store;
                        temp_t = curr->time;

                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        curr->me_but_in_store= file_in;
                        curr->am_dead = 0;
                        file_in->me_but_in_cache = curr;
                        curr->time = temp->time;

                        temp->file_name = temp_fl;
                        temp->me_but_in_store = temp_me;
                        temp_me->me_but_in_cache = NULL;
                        file_in = temp_me;
                        temp->ref = &temp_me->ref;
                        temp->time = temp_t;
                        //! LOCKS RELEASED
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                    } else {
                        MY_CACHE->buckets[index]->threads_in--;
                        // printf("TIME: %ld------%ld FILES: %s ----- %s\n", temp->time, file_in->time, (char*)file_in->key, temp->file_name);
                        //!LOCKS RELEASED
                        UNLOCK(&curr->mutex);
                        UNLOCK(UNLOCK_ME);
                        free(temp);
                        return 0;
                    }
                    int old_ind = index;
                    index = _get_level(ht->incr_entr - *(temp->ref));
                    MY_CACHE->buckets[old_ind]->threads_in--;
                    MY_CACHE->buckets[index]->threads_in++;
                    #ifdef DEBUG
                        assert(file_in != NULL);
                    #endif // DEBUG
                    //!LOCK RELEASED if am_dead == 0 and new_ind > max_out
                    if((old_ind == (index && ht->buckets[index]->num_dead - MY_CACHE->buckets[index]->threads_in-1)) <= 0){ 
                        break;
                    }
                    if((old_ind == index && ht->buckets[index]->num_dead - MY_CACHE->buckets[index]->threads_in-1) > 0){
                        if(tail_or_head){
                            curr=curr->prev;
                        } else curr = curr->next;
                        continue;
                    }
                    if(get_rand_diff() < 0){
                        tail_or_head = 1;
                        curr = ht->buckets[index]->tail;
                        #ifdef DEBUG
                        assert(curr != NULL);
                        #endif // DEBUG
                        continue;
                    } else {
                        tail_or_head = 0;
                        curr = ht->buckets[index]->head;
                        #ifdef DEBUG
                        assert(curr != NULL);
                        #endif // DEBUG
                        continue;
                    }
                    //!LOCK RELEASED if < max_and_you_out
                }else{
                    UNLOCK(&curr->mutex);
                }
            }
        }
        if(tail_or_head){
            curr = curr->prev;
        } else curr = curr->next;
        continue;
    }
    //We should finish here if we didn't have enough space in the row
    //to insert an entry, so we give a bust to the file to fit an arbitrary row that ha an empty slot
    MY_CACHE->buckets[index]->threads_in--;
    if(MY_CACHE->nfiles >= MY_CACHE->START_INI_CACHE-1 || FILES_STORAGE->total_bytes >= MY_CACHE->MAX_SPACE_AVAILABLE){
        find_victim(ht, ret, temp);
        free(temp);
        return 0;
    }
    index = _find_dead_helper(ht, index);
    if(index == -1){
        find_victim(ht, ret, temp);
        free(temp);
        return 0;
    };
    curr = tail_or_head == 0 ? ht->buckets[index]->head->next : ht->buckets[index]->tail->prev;

    while((errno = 0, _till_end(curr, file_in, tail_or_head, index)) == NULL){
        sched_yield();
        if(errno == ENODATA){
            free(temp);
            return 0;
        }
        index = _find_dead_helper(ht, index);
        if(index == -1) return -1;
        curr = tail_or_head == 0 ? ht->buckets[index]->head : ht->buckets[index]->tail;
        if(index == -1 || MY_CACHE->nfiles >= MY_CACHE->START_INI_CACHE-1 || FILES_STORAGE->total_bytes >= MY_CACHE->MAX_SPACE_AVAILABLE){
        find_victim(ht, ret, temp);
        free(temp);
        return 0;
    }
    }
    free(temp);
    return 0;
}



/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param file_name -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

int cach_hash_insert_bind(cach_hash_t *ht, icl_entry_t *file_in, pointers *ret)
{
    ht->incr_entr++;
    int index = _get_level(ht->incr_entr-file_in->ref);
    cach_entry_t *temp_ent = bind_two_tables_create_entry(&*file_in, index);
    if(temp_ent == NULL){
        return -1;
    }
   int res =  _helper_ins_rand(ht, temp_ent, file_in, index, ret);
   if(res == -1){
        return -1;
   }
   return 0;

}

int
cach_hash_destroy(cach_hash_t *ht)
{   
    if(!ht) return -1;
    cach_entry_t *curr, *next;
    int res = 0;
    for(size_t i = 0; i <ht->nbuckets; i++){
        for(curr = ht->buckets[i]->head; curr != NULL; ){
            next = curr->next;
            SYSCALL_EXIT(pthread_mutex_destroy, res, pthread_mutex_destroy(&curr->mutex), "Unexpected behavior, bye\n", NULL);
            if(res != 0) {
                fprintf(stderr, "Error: %s\n", strerror(res));
            }
            free(curr);
            curr=next;
        }
        SYSCALL_EXIT(pthread_mutex_destroy, res, pthread_mutex_destroy(&ht->buckets[i]->mutex_for_cleanup), "Unexpected behavior, bye\n", NULL);
        free(ht->buckets[i]);
    }

    free(ht->buckets);
    free(ht);
    return 0;
}

/**
 * Dump the hash table's contents to the given file pointer.
 *
 * @param stream -- the file to which the hash table should be dumped
 * @param ht -- the hash table to be dumped
 *
 * @returns 0 on success, -1 on failure.
 */

int
cach_hash_dump(FILE* stream, cach_hash_t* ht)
{
    cach_entry_t *bucket, *curr;
    int i;
    if(!ht) return -1;

    for(i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i]->head;
        if(ht->buckets[i]){
            fprintf(stream, "ROW SUMMARY: Num_dead: %ld Group: %d Max_and_you_out: %d Min_and_you_in: %d NFiles_row: %ld\n", ht->buckets[i]->num_dead, ht->buckets[i]->group, ht->buckets[i]->max_and_you_out, ht->buckets[i]->min_and_you_in,  ht->buckets[i]->nfiles_row);
        }
        for(curr=bucket; curr!=NULL; curr=curr->next) {
            if(!curr->am_dead && curr->file_name){
                fprintf(stream, "%s\n", curr->file_name);
            }
        }
    }
    fprintf(stream, "SUMMARY: nbuckets: %d nfiles: %ld\n", ht->nbuckets, ht->nfiles);
    return 0;
}

int
_helper_find_vic(cach_entry_t *curr, int tail_or_head, pointers *ret, cach_hash_t *ht, int index, cach_entry_t *repl){
    while(curr != NULL){
        if(!curr->am_dead && !curr->me_but_in_store->am_being_used){
            #ifdef DEBUG
            assert(curr->me_but_in_store->am_being_used >= 0);
            #endif // DEBUG
            if(TRYLOCK(&curr->mutex) == 0){
                //!LOCK ACQUIRED
                if(!curr->am_dead){
                    pthread_mutex_t *THIS_IS_LCK = &curr->me_but_in_store->wr_dl_ap_lck;
                    if(!TRYLOCK(THIS_IS_LCK)){
                        //!LOCK ACQUIRED
                        curr->am_dead = 0;
                        curr->me_but_in_store->empty = 1;
                        ret->been_modified = curr->me_but_in_store->been_modified;
                        ret->key = curr->me_but_in_store->key;
                        dprintf(ARG_LOG_TH.pipe[WRITE], "VICTIM: %s BEEN MODIFIED: %d\n", (char*)ret->key, curr->me_but_in_store->been_modified);
                        curr->me_but_in_store->key = NULL;
                        ret->data = curr->me_but_in_store->data;
                        curr->me_but_in_store->data = NULL;
                        ret->size_data = curr->me_but_in_store->ptr_tail+1;
                        LOCK(&FILES_STORAGE->stat_lck);
                        FILES_STORAGE->nentries--;
                        FILES_STORAGE->total_victims++;
                        if(ret->data){
                            long long test = FILES_STORAGE->total_bytes;
                            FILES_STORAGE->total_bytes -= (curr->me_but_in_store->ptr_tail+1)*sizeof(u_int8_t);
                            #ifdef DEBUG
                            assert(test > FILES_STORAGE->total_bytes);
                            #endif // DEBUG
                        }
                        UNLOCK(&FILES_STORAGE->stat_lck);
                        curr->me_but_in_store->ref = 0;
                        curr->me_but_in_store->am_being_used = 0;
                        curr->me_but_in_store->me_but_in_cache = NULL;
                        curr->me_but_in_store->OWNER = 0;
                        curr->me_but_in_store->O_LOCK = 0;
                        curr->me_but_in_store->ptr_tail = 0;
                        curr->me_but_in_store->open = 0;
                        curr->me_but_in_store->time = 0;
                        curr->me_but_in_store->been_modified = 0;
                        curr->file_name = repl->file_name;
                        curr->me_but_in_store = repl->me_but_in_store;
                        curr->ref = &repl->me_but_in_store->ref;
                        curr->me_but_in_store->me_but_in_cache = curr;
                        curr->time = repl->me_but_in_store->time;
                        //!LOCK RELEASED 
                        UNLOCK(THIS_IS_LCK);
                        UNLOCK(&curr->mutex);
                        return 0;
                    }
                }
                //!LOCK RELEASED if victim is dead
                UNLOCK(&curr->mutex);
            }
        }
        if(tail_or_head){
            curr = curr->prev;
        } else curr = curr->next;
    }
    return -1;
}

/**
 * @param ht -- cache that will be used to look for a victim
 * @param ret -- key and data will be saved here.
 * @return -- 0 success, -1 failure
 */
int
find_victim(cach_hash_t *ht, pointers *ret, cach_entry_t *repl){
    volatile int check = -1;
    int index = 0;

    index =ht->NUM_GROUP-1;
    //0 for head, 1 tail
    while(check){
        int tail_or_head = 0;
        cach_entry_t *curr = NULL;
        tail_or_head = get_rand_diff() < 0 ? 0 : 1;
        curr = tail_or_head == 0 ? ht->buckets[index]->head : ht->buckets[index]->tail;
        check =_helper_find_vic(curr, tail_or_head, ret, ht, index, repl);
        index--;
        if(index < 0) index = MY_CACHE->NUM_GROUP - 1;
    }
    return 0;
}

int
_helper_find_vic_no_rep(cach_entry_t *curr, int tail_or_head, pointers *ret, cach_hash_t *ht, int index, ssize_t *num_bytes){
    while(curr != NULL){
        if(!curr->am_dead && curr->me_but_in_store->am_being_used<=0 && curr->me_but_in_store->ptr_tail>0){
            #ifdef DEBUG
            assert(curr->me_but_in_store->am_being_used >= 0);
            #endif // DEBUG
            if(TRYLOCK(&curr->mutex) == 0){
                //!LOCK ACQUIRED
                if(!curr->am_dead){
                    pthread_mutex_t *THIS_IS_LCK = &curr->me_but_in_store->wr_dl_ap_lck;
                    if(!TRYLOCK(THIS_IS_LCK)){
                        //!LOCK ACQUIRED
                        curr->am_dead = 1;
                        curr->me_but_in_store->empty = 1;
                        ret->been_modified = curr->me_but_in_store->been_modified;
                        ret->key = curr->me_but_in_store->key;
                        dprintf(ARG_LOG_TH.pipe[WRITE], "VICTIM: %s BEEN MODIFIED: %d\n", (char*)ret->key, curr->me_but_in_store->been_modified);
                        curr->me_but_in_store->key = NULL;
                        ret->data = curr->me_but_in_store->data;
                        curr->me_but_in_store->data = NULL;
                        ret->size_data = curr->me_but_in_store->ptr_tail+1;
                        LOCK(&FILES_STORAGE->stat_lck);
                        MY_CACHE->nfiles--;
                        MY_CACHE->buckets[index]->num_dead++;
                        MY_CACHE->buckets[index]->nfiles_row--;
                        FILES_STORAGE->nentries--;
                        FILES_STORAGE->total_victims++;
                        if(ret->data){
                            long long test = FILES_STORAGE->total_bytes;
                            *num_bytes =  (curr->me_but_in_store->ptr_tail+1)*sizeof(u_int8_t);
                            FILES_STORAGE->total_bytes -= (*num_bytes);
                            #ifdef DEBUG
                            assert(test > FILES_STORAGE->total_bytes);
                            #endif // DEBUG
                        }
                        UNLOCK(&FILES_STORAGE->stat_lck);
                        curr->me_but_in_store->ref = 0;
                        curr->me_but_in_store->ptr_tail=0;
                        curr->me_but_in_store->am_being_used = 0;
                        curr->me_but_in_store->me_but_in_cache = NULL;
                        curr->me_but_in_store->OWNER = 0;
                        curr->me_but_in_store->O_LOCK = 0;
                        curr->me_but_in_store->ptr_tail = 0;
                        curr->me_but_in_store->open = 0;
                        curr->me_but_in_store->time = 0;
                        curr->me_but_in_store->been_modified = 0;
                        curr->file_name = NULL;
                        curr->me_but_in_store = NULL;
                        curr->ref = 0;
                        curr->time = 0;
                        //!LOCK RELEASED 
                        UNLOCK(THIS_IS_LCK);
                        UNLOCK(&curr->mutex);
                        return 0;
                    }
                }
                //!LOCK RELEASED if victim is dead
                UNLOCK(&curr->mutex);
            }
        }
        if(tail_or_head){
            curr = curr->prev;
        } else curr = curr->next;
    }
    return -1;
}

/**
 * @param ht -- cache that will be used to look for a victim
 * @param ret -- key and data will be saved here.
 * @param time_to_quit: signal handler variable.
 * @return -- Numbers of bytes deleted.
 */
ssize_t
find_victim_no_rep(cach_hash_t *ht, pointers *ret, volatile sig_atomic_t *time_to_quit){
    volatile int check = -1;
    int index = 0;
    ssize_t num_bytes = 0;
    index =ht->NUM_GROUP-1;
    //0 for head, 1 tail
    while(check && *time_to_quit != 1){
        int tail_or_head = 0;
        cach_entry_t *curr = NULL;
        tail_or_head = get_rand_diff() < 0 ? 0 : 1;
        curr = tail_or_head == 0 ? ht->buckets[index]->head : ht->buckets[index]->tail;
        check =_helper_find_vic_no_rep(curr, tail_or_head, ret, ht, index, &num_bytes);
        index--;
        if(index < 0) index = MY_CACHE->NUM_GROUP - 1;
    }
    return num_bytes;
}


void print_info (cach_hash_t *ht){
    printf("nfiles: %ld size buck: %zd\n", ht->nfiles, ht->incr_entr);
    for(int i=0; i<ht->NUM_GROUP; i++){
        printf("group: %d num_dead: %ld max_and_you_out: %d min_and_you_in: %d nfiles_row: %ld \n", ht->buckets[i]->group, ht->buckets[i]->num_dead, ht->buckets[i]->max_and_you_out, ht->buckets[i]->min_and_you_in, ht->buckets[i]->nfiles_row);
        for(cach_entry_t *curr = ht->buckets[i]->head; curr != NULL; curr = curr->next){
            printf("am_dead: %ld  group: %d file_name: %s \n", curr->am_dead, curr->group, curr->file_name);
        }
    }
}

#ifdef DEBUG_NEW
cach_hash_t *MY_CACHE;
icl_hash_t *FILES_STORAGE;
int main(int argc, char **argv){
    // cach_hash_t *ht = cach_hash_create(2, NULL);
    #define TEST 150
    #define EXTRA 10000
    struct Cache_settings sett;
    sett.NUM_GROUP = 10;
    sett.CACHE_RANGE = 3;
    sett.START_INI_CACHE = TEST;
    sett.EXTRA_CACHE_SPACE = 0.049;
    FILES_STORAGE = NULL;
    MY_CACHE = NULL;

    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(sett.START_INI_CACHE, hash_pjw, string_compare);
    char rand_t[TEST+EXTRA][5] = {"\0"};
    FILE *fl = fopen("cach.txt", "a+");
    FILE *fl_sto = fopen("storage.txt", "a+");
            
    srand(time(NULL));
    srand48(~((pthread_self() ^ (2342323522  | getpid())) << 22));
    for(size_t i=0; i <TEST+EXTRA; i++){
        rand_string(rand_t[i], 5);
        pointers ret_point;
        memset(&ret_point, 0, sizeof(ret_point));
        if((icl_hash_insert(FILES_STORAGE, (void *) &(rand_t[i]), NULL, &ret_point), errno = 0) != 0 && errno != EBUSY){
            fprintf(stderr, "Error fatal in icl_hash_insert\n");
            pthread_exit(NULL);
        }
        if(i > (int) TEST/2 && i % 2 == 1){
            char *rand_rem = rand_t[lrand48() % i];
            fprintf(fl_sto, "RANDOM SELECTED TO REMOVE: %s NFILES: %d NENTR: %d\n", rand_rem, MY_CACHE->nfiles, FILES_STORAGE->nentries);
            icl_hash_delete_ext(FILES_STORAGE, rand_rem, NULL, NULL, 0);
            fprintf(fl_sto, "RANDOM SELECTED TO REMOVE AFTER: %s NFILES: %d NENTR: %d\n", rand_rem, MY_CACHE->nfiles, FILES_STORAGE->nentries);

        }
        if(ret_point.key) fprintf(fl_sto, "REMOVED: %s\n", (char*)ret_point.key);
        if(i == TEST-1){
            fprintf(fl, "BEFORE-------------REMOVAL\n");
            fprintf(fl_sto, "BEFORE-------------REMOVAL\n");
            cach_hash_dump(fl, MY_CACHE);
            icl_hash_dump(fl_sto, FILES_STORAGE);
        }
    }
    fprintf(fl, "AFTER-------------REMOVAL\n");
    fprintf(fl_sto, "AFTER-------------REMOVAL\n");
    cach_hash_dump(fl, MY_CACHE);
    icl_hash_dump(fl_sto, FILES_STORAGE);
    fclose(fl);
    fclose(fl_sto);
    // print_storage(FILES_STORAGE);
    // print_info(MY_CACHE);
    printf("CACHE: %d-----STOR ENT: %d\n", MY_CACHE->nfiles, FILES_STORAGE->nentries);
    cach_hash_destroy(MY_CACHE);
    icl_hash_destroy(FILES_STORAGE, NULL, NULL);
}

#endif // DEBUG