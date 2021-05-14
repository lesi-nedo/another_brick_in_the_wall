
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "new.h"
#include "../../includes/util.h"
#include <pthread.h>
#include <limits.h>
#include "../../includes/thread_for_log.h"
cach_hash_t *MY_CACHE;
icl_hash_t *FILES_STORAGE;


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

    ht = (cach_hash_t*) malloc(sizeof(cach_hash_t));
    if(!ht) return NULL;

    ht->nentries = 0;
    ht->incr_entr = 0;
    ht->buckets = (cach_fd_t**)malloc(sett.NUM_GROUP * sizeof(cach_fd_t*));
    for(size_t i=0; i<sett.NUM_GROUP; i++){ 
        ht->buckets[i] =(cach_fd_t*)malloc(sizeof(cach_fd_t));
        CHECK_EQ_EXIT(malloc, ht->buckets[i], NULL, "I'm out, bye.\n", NULL);
    }
    if(!ht->buckets) return NULL;
    ht->NUM_GROUP = sett.NUM_GROUP;
    ht->CACHE_RANGE = sett.CACHE_RANGE;
    ht->MAX_LAST_LEVEL = sett.NUM_GROUP*sett.CACHE_RANGE;
    ht->nfiles = 0;
    ht->START_INI_CACHE = sett.START_INI_CACHE + (sett.START_INI_CACHE*sett.EXTRA_CACHE_SPACE);
    for(i=0;i<ht->NUM_GROUP;i++){
        if(ht->buckets[i] == NULL) printf("SUKKKA\n");
        ht->buckets[i]->group = i;
        ht->buckets[i]->head = NULL;
        ht->buckets[i]->tail = NULL;
        ht->buckets[i]->nfiles_row = 0;
        ht->buckets[i]->num_dead = 0;
        SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&(ht->buckets[i]->mutex_for_cleanup), NULL), "This was not expected, well it is a shame maybe try again.\n", NULL);
        ht->buckets[i]->min_and_you_in = sett.CACHE_RANGE*i;
        ht->buckets[i]->max_and_you_out = sett.CACHE_RANGE*(i+1);
    }
    for(size_t i = 0; i < ht->START_INI_CACHE+1; i++){
        if(i == ht->NUM_GROUP*pos_row) pos_row++;
        unsigned int hashed = i % ht->NUM_GROUP;
        cach_entry_t *to_ins = (cach_entry_t *)malloc(sizeof(cach_entry_t));
        CHECK_EQ_EXIT(malloc, to_ins, NULL, "I'm out, maybe try again, bye\n", NULL);
        to_ins->am_dead = 1;
        to_ins->ref = NULL;
        to_ins->group = hashed;
        to_ins->file_name = NULL;
        SYSCALL_EXIT(pthread_mutex_init, res, pthread_mutex_init(&to_ins->mutex, NULL), "Wasn't my fault, however I'm sorry <3 <3\n", NULL);
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
*_till_end(cach_entry_t *curr, icl_entry_t *file_in, int tail_or_head){
    while(curr){
        if(curr->am_dead){
            if(!TRYLOCK(&curr->mutex)){
                //!LOCK ACQUIRED
                if(curr->am_dead){
                    curr->am_dead = 0;
                    curr->ref = &file_in->ref;
                    curr->file_name = file_in->key;
                    file_in->me_but_in_cache = curr;
                    //!LOCK RELEASED if am_dead == 1
                    UNLOCK(&curr->mutex);
                    return curr;
                }
                 //!LOCK RELEASED if am_dead == 1
                UNLOCK(&curr->mutex);
            }
        }
        if(tail_or_head){
            curr = curr->prev;
        } else curr = curr->next;
    }
    return NULL;
}


/**
 * @param curr -- starting point from which will be searched
 * @param temp -- temporary entries to hold all variables information to be insert into cache
 * @return -- 0 success 
 */
int
 _helper_ins_rand(cach_hash_t *ht, cach_entry_t *temp, icl_entry_t *file_in, int index){
    //0 starts from head 1  from tail.
    int tail_or_head = get_rand_diff() < 0 ? 1 : 0;
    ht->nfiles++;
    cach_entry_t *curr = tail_or_head == 0? ht->buckets[index]->head: ht->buckets[index]->tail;
    if(ht == NULL || temp == NULL || file_in == NULL){
        fprintf(stderr, "\033[1;31mFatal ERROR\033[1;37m in _helper_ins_rand, one of the argument is null, congratulation your code has a bug.\033[0;37m\n");
        exit(EXIT_FAILURE);
    }
    while(curr != NULL){
        if(curr->am_dead){
            //JUMPS TO THE END BECAUSE WE NEED TO DELETE TEMP
            if(file_in->need_to_be_removed) goto delete_temp;
            if(!TRYLOCK(&curr->mutex)){
                //!LOCK ACQUIRED
                if(curr->am_dead){
                    curr->am_dead = 0;
                    curr->ref = temp->ref;
                    curr->file_name = temp->file_name;
                    file_in->me_but_in_cache = curr;
                    free(temp);
                    ht->buckets[index]->nfiles_row++;
                    ht->buckets[index]->num_dead--;
                    //!LOCK RELEASED if am_dead == 1
                     UNLOCK(&curr->mutex);
                    return 0;
                }
                //!LOCK RELEASED if am_dead == 0
                UNLOCK(&curr->mutex);
                if(file_in->need_to_be_removed) goto delete_temp;
                if(tail_or_head){
                    curr = curr->prev;
                } else curr = curr->next;
                continue;
            } else {
                if(file_in->need_to_be_removed) goto delete_temp;
                if(tail_or_head){
                    curr = curr->prev;
                } else curr = curr->next;
                continue;
            }
        } else if((ht->incr_entr - *(curr->ref)) >= ht->buckets[index]->max_and_you_out){
            if(file_in->need_to_be_removed) goto delete_temp;
            if(!TRYLOCK(&curr->mutex)){
                //!LOCK ACQUIRED
                //rechecking the condition
                if((ht->incr_entr - *(curr->ref)) >= ht->buckets[index]->max_and_you_out){
                    //rechecking the first condition
                    if(curr->am_dead){
                        curr->am_dead = 0;
                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        file_in->me_but_in_cache = curr;
                        ht->buckets[index]->nfiles_row++;
                        ht->buckets[index]->num_dead--;
                        free(temp);
                        //!LOCK RELEASED if am_dead == 1
                        UNLOCK(&curr->mutex);
                        return 0;
                    } else{
                        char *temp_fl = NULL;

                        temp_fl = curr->file_name;
                        #ifdef DEBUG
                            assert(!curr->am_dead);
                        #endif // DEBUG 
                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        file_in->me_but_in_cache = curr;
                        temp->file_name = temp_fl;
                        //!LOCK RELEASED if am_dead == 0 and new_ind > max_out
                        UNLOCK(&curr->mutex);
                        file_in = icl_hash_find(FILES_STORAGE, temp->file_name);
                        temp->ref = &file_in->ref;
                        int old_ind = index;
                        index = _get_level(ht->incr_entr - *(temp->ref));
                        #ifdef DEBUG
                            assert(file_in != NULL);
                            assert(index < 16 && index >= 0);
                        #endif // DEBUG
                        if(file_in->need_to_be_removed) goto delete_temp;
                        if(old_ind == index && ht->buckets[index]->num_dead <= 0)break;
                        if(old_ind == index && ht->buckets[index]->num_dead > 0){
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
                    }
                } else {
                    //!LOCK RELEASED if < max_and_you_out
                    UNLOCK(&curr->mutex);
                    if(file_in->need_to_be_removed) goto delete_temp;
                    if(tail_or_head){
                        curr = curr->prev;
                    } else curr = curr->next;
                    continue;
                }
            } else {
                if(tail_or_head){
                    curr = curr->prev;
                } else curr = curr->next;
                if(file_in->need_to_be_removed) goto delete_temp;
                continue;
            }
        }else if((ht->incr_entr - *(curr->ref)) < ht->buckets[index]->min_and_you_in){
            if(!TRYLOCK(&curr->mutex)){
                //!LOCK ACQUIRED
                //rechecking the condition
                if((ht->incr_entr - *(curr->ref)) < ht->buckets[index]->max_and_you_out){
                    //rechecking the first condition
                    if(curr->am_dead){
                        curr->am_dead = 0;
                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        file_in->me_but_in_cache = curr;
                        ht->buckets[index]->nfiles_row++;
                        ht->buckets[index]->num_dead--;
                        free(temp);
                        //!LOCK RELEASED if am_dead == 1
                        UNLOCK(&curr->mutex);
                        return 0;
                    } else{
                        char *temp_fl = NULL;
                        temp_fl = curr->file_name;
                        #ifdef DEBUG
                            assert(curr->am_dead);
                        #endif // DEBUG 
                        curr->ref = temp->ref;
                        curr->file_name = temp->file_name;
                        file_in->me_but_in_cache = curr;
                        temp->file_name = temp_fl;
                        //!LOCK RELEASED if am_dead == 0 and new_ind > max_out
                        UNLOCK(&curr->mutex);
                        file_in = icl_hash_find(FILES_STORAGE, temp->file_name);
                        temp->ref = &file_in->ref;
                        int old_ind = index;
                        index = _get_level(ht->incr_entr - *(temp->ref));
                        #ifdef DEBUG
                            assert(file_in != NULL);
                        #endif // DEBUG
                        if(old_ind == index && ht->buckets[index]->num_dead <= 0) break;
                        if(old_ind == index && ht->buckets[index]->num_dead > 0){
                            if(tail_or_head){
                                curr=curr->prev;
                            } else curr = curr->next;
                            continue;
                        }
                        if(file_in->need_to_be_removed) goto delete_temp;
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
                    }
                } else {
                    //!LOCK RELEASED if < max_and_you_out
                    UNLOCK(&curr->mutex);
                    if(file_in->need_to_be_removed) goto delete_temp;
                    if(tail_or_head){
                        curr = curr->prev;
                    } else curr = curr->next;
                    continue;
                }
            } else {
                if(tail_or_head){
                    curr = curr->prev;
                } else curr = curr->next;
                if(file_in->need_to_be_removed) goto delete_temp;
                continue;
            }
        }else {
            if(tail_or_head){
                    curr = curr->prev;
                } else curr = curr->next;
                if(file_in->need_to_be_removed) goto delete_temp;
                continue;
        }
    }


    //We should finish here if we didn't have enough space in the row
    //to insert an entry, so we give a bust to the file to fit an arbitrary row that ha an empty slot
    while(index >=0 && ht->buckets[index]->num_dead <= 0) index--;
    if(index < 0){
        index = 0;
        while(index < MY_CACHE->NUM_GROUP && ht->buckets[index]->num_dead <= 0) index++;
    }
    if(index >= MY_CACHE->NUM_GROUP){
        fprintf(stderr, "\033[1;31mCongratulations you've discovered a bug, there is no sense to continue. I'm out.\n");
        exit(EXIT_FAILURE);
    }
    curr = tail_or_head == 0 ? ht->buckets[index]->head->next : ht->buckets[index]->tail->prev;
    if(file_in->need_to_be_removed) goto delete_temp;
    while(_till_end(&*curr, &*file_in, tail_or_head) == NULL){
        while(index >=0 && ht->buckets[index]->num_dead <= 0) index--;
            printf("%d---%d\n", index, ht->buckets[index]->num_dead);
        if(index < 0){
            index = 0;
            while(index < MY_CACHE->NUM_GROUP && ht->buckets[index]->num_dead <= 0) index++;
        }
        if(index >= MY_CACHE->NUM_GROUP){
            fprintf(stderr, "\033[1;31mCongratulations you've discovered a bug, there is no sense to continue. I'm out.\n");
            exit(EXIT_FAILURE);
        }
        if(file_in->need_to_be_removed) goto delete_temp;
        curr = tail_or_head == 0 ? ht->buckets[index]->head : ht->buckets[index]->tail;
    }
    file_in->ref = ht->incr_entr -  ht->buckets[index]->min_and_you_in;
    ht->buckets[index]->nfiles_row++;
    ht->buckets[index]->num_dead--;
    free(temp);
    return 0;
    delete_temp:
        file_in->need_to_be_removed = 0;
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
cach_entry_t
* cach_hash_insert_bind(cach_hash_t *ht, icl_entry_t *file_in)
{
    if(MY_CACHE->nfiles < MY_CACHE->START_INI_CACHE){
        ht->incr_entr++;
        int index = _get_level(ht->incr_entr-file_in->ref);
        cach_entry_t *temp_ent = bind_two_tables_create_entry(&*file_in, index);
        _helper_ins_rand(&*ht, temp_ent, &*file_in, index);
    
    } else printf("TO MUCH BRO: ");
    return NULL;
}

void print_info (cach_hash_t *ht){
    printf("nentries: %d nfiles: %d size buck: %zd\n", ht->nentries, ht->nfiles, ht->incr_entr);
    for(int i=0; i<ht->NUM_GROUP; i++){
        printf("group: %d num_dead: %d max_and_you_out: %d min_and_you_in: %d nfiles_row: %d \n", ht->buckets[i]->group, ht->buckets[i]->num_dead, ht->buckets[i]->max_and_you_out, ht->buckets[i]->min_and_you_in, ht->buckets[i]->nfiles_row);
        for(cach_entry_t *curr = ht->buckets[i]->head; curr != NULL; curr = curr->next){
            printf("am_dead: %d  group: %d file_name: %s \n", curr->am_dead, curr->group, curr->file_name);
        }
    }
}

//TODO:TEST IN MULTI THREADING AND ADD API REMOVE ETC
int main(int argc, char **argv){
    // cach_hash_t *ht = cach_hash_create(2, NULL);
    struct Cache_settings sett;
    sett.NUM_GROUP = 16;
    sett.CACHE_RANGE = 3;
    sett.START_INI_CACHE = 100;
    sett.EXTRA_CACHE_SPACE = 0.2;

    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(sett.START_INI_CACHE, triple32, string_compare);
    char rand_t[122][5];
    srand(time(NULL));
    srand48(~((pthread_self() ^ (2342323522  | getpid())) << 22));
    for(size_t i=0; i <122; i++){
        rand_string(rand_t[i], 5);
        icl_hash_insert(FILES_STORAGE, (void *) &(rand_t[i]), NULL);
    }
    print_storage(FILES_STORAGE);
    print_info(MY_CACHE);
    printf("%d\n", MY_CACHE->nfiles);
}