
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
        CHECK_EQ_EXIT(malloc, ht->buckets[i], NULL, "I'm out, bye.\n", NULL);
    }
    if(!ht->buckets) return NULL;
    ht->nbuckets = sett.NUM_GROUP;
    ht->NUM_GROUP = sett.NUM_GROUP;
    ht->CACHE_RANGE = sett.CACHE_RANGE;
    ht->MAX_LAST_LEVEL = sett.NUM_GROUP*sett.CACHE_RANGE;
    ht->nfiles = 0;
    ht->START_INI_CACHE = sett.START_INI_CACHE;
    extr = (int) (sett.START_INI_CACHE*sett.EXTRA_CACHE_SPACE);;
    for(i=0;i<ht->NUM_GROUP;i++){
        ht->buckets[i]->group = i;
        ht->buckets[i]->head = NULL;
        ht->buckets[i]->tail = NULL;
        ht->buckets[i]->nfiles_row = 0;
        ht->buckets[i]->num_dead = 0;
        SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&(ht->buckets[i]->mutex_for_cleanup), NULL), "This was not expected, well it is a shame maybe try again.\n", NULL);
        ht->buckets[i]->min_and_you_in = sett.CACHE_RANGE*i;
        ht->buckets[i]->max_and_you_out = sett.CACHE_RANGE*(i+1);
    }
    for(size_t i = 0; i < ht->START_INI_CACHE+extr; i++){
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

signed int
get_rand(){
    long res1= 0; 
    struct drand48_data buff;
    unsigned long seed = get_seed(pthread_self()^getpid());
    srand48_r(seed, &buff);
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
        fprintf(stderr, "\033[1;31mCongratulations you've discovered a bug, \033[1;37mthere was no space for a new entry that means function find victim does no work properly. There is no sense to continue. \033[1;31mI'm out.\033[0;37m\n");
        exit(EXIT_FAILURE);
    }
    curr = tail_or_head == 0 ? ht->buckets[index]->head->next : ht->buckets[index]->tail->prev;
    if(file_in->need_to_be_removed) goto delete_temp;
    while(_till_end(&*curr, &*file_in, tail_or_head) == NULL){
        while(index >=0 && ht->buckets[index]->num_dead <= 0) index--;
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
    signed int diff = ht->incr_entr -  ht->buckets[index]->min_and_you_in;
    file_in->ref = diff <= ~diff+1 ? 0 : diff;
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
* cach_hash_insert_bind(cach_hash_t *ht, icl_entry_t *file_in, pointers *ret)
{
    ht->incr_entr++;
    int index = _get_level(ht->incr_entr-file_in->ref);
    cach_entry_t *temp_ent = bind_two_tables_create_entry(&*file_in, index);
    if(MY_CACHE->nfiles < MY_CACHE->START_INI_CACHE){

        _helper_ins_rand(&*ht, temp_ent, &*file_in, index);
    
    } else {
        int res = find_victim(&*ht, &*ret);
        _helper_ins_rand(&*ht, temp_ent, &*file_in, index);
        #ifdef DEBUG
            assert(res == 0);
        #endif // DEBUG

    }
    return NULL;
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
            fprintf(stream, "ROW SUMMARY: Num_dead: %d Group: %d Max_and_you_out: %d Min_and_you_in: %d NFiles_row: %d\n", ht->buckets[i]->num_dead, ht->buckets[i]->group, ht->buckets[i]->max_and_you_out, ht->buckets[i]->min_and_you_in, ht->buckets[i]->nfiles_row);
        }
        for(curr=bucket; curr!=NULL; ) {
            if(!curr->am_dead && curr->file_name)
                fprintf(stream, "cach_hash_dump: file_name: %s: ref: %lu group: %d \n", curr->file_name, *curr->ref, curr->group);
            curr=curr->next;
        }
    }

    return 0;
}

int
_helper_find_vic(cach_entry_t *curr, int tail_or_head, pointers *ret, cach_hash_t *ht, int index){
    while(curr != NULL){
        if(!curr->am_dead){
            if(!TRYLOCK(&curr->mutex)){

                if(!curr->am_dead){
                    //!LOCK ACQUIRED
                    if((errno = 0, remove_victim(FILES_STORAGE, (void *)curr->file_name, &*ret)) == -1){
                        //!LOCK RELEASED if victim is busy
                        UNLOCK(&curr->mutex);
                        if(errno == ETXTBSY){
                            if(tail_or_head){
                                curr = curr->prev;
                            } else curr = curr->next;
                            continue;
                        } else {
                            fprintf(stderr, "In _helper_find_vic calling remove_victim. ERROR: %s\n", strerror(errno));
                            pthread_exit(curr->file_name);
                        }
                    }
                    ht->nfiles--;
                    ht->buckets[index]->nfiles_row--;
                    ht->buckets[index]->num_dead++;
                    curr->am_dead = 1;
                    curr->file_name = NULL;
                    curr->ref = NULL;
                    //!LOCK RELEASED if victim is busy
                    UNLOCK(&curr->mutex);
                    return 0;
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
find_victim(cach_hash_t *ht, pointers *ret){
    int check = -1;
    int index = 0;
    int mod_ptr =  0;
    cach_entry_t *old = NULL;

    index =ht->NUM_GROUP-1;
    mod_ptr = ht->buckets[index]->nfiles_row <= 1 ? 1 : ht->buckets[index]->nfiles_row -1;
    //0 for head, 1 tail
    while(index >= 0){
        int tail_or_head = 0;
        cach_entry_t *curr = NULL;
        int diff_p = 0;
        int ind = 0;

        curr = tail_or_head == 0 ? ht->buckets[index]->head : ht->buckets[index]->tail;
        tail_or_head = get_rand_diff() < 0 ? 0 : 1;
        diff_p = ht->buckets[index]->head - ht->buckets[index]->head->next;
        ind = get_rand() % mod_ptr;
        old = curr;
        if(tail_or_head){
            curr = (curr-(ind*diff_p));
        } else curr = curr + (ind*diff_p);
        if((check =_helper_find_vic(curr, tail_or_head, &*ret, &*ht, index)) == -1){
            if(tail_or_head){
                if(old->next == NULL){ 
                    index--;
                    continue;
                }
                tail_or_head = 0;
                curr = old - (ind*diff_p);
                check = _helper_find_vic(curr, tail_or_head, &*ret, &*ht, index);
            } else {
                if(old->next == NULL){ 
                    index--;
                    continue;
                }
                tail_or_head = 1;
                curr = old + (ind*diff_p);
                check = _helper_find_vic(curr, tail_or_head, &*ret, &*ht, index);
            }
        }
        if(check == 0) return 0;
        index--;
    }
    #ifdef DEBUG
        fprintf(stderr, "BUG: find_victim didn't not find a victim");
        assert(1);
    #endif // DEBUG
    return -1;
}


void print_info (cach_hash_t *ht){
    printf("nfiles: %d size buck: %zd\n", ht->nfiles, ht->incr_entr);
    for(int i=0; i<ht->NUM_GROUP; i++){
        printf("group: %d num_dead: %d max_and_you_out: %d min_and_you_in: %d nfiles_row: %d \n", ht->buckets[i]->group, ht->buckets[i]->num_dead, ht->buckets[i]->max_and_you_out, ht->buckets[i]->min_and_you_in, ht->buckets[i]->nfiles_row);
        for(cach_entry_t *curr = ht->buckets[i]->head; curr != NULL; curr = curr->next){
            printf("am_dead: %d  group: %d file_name: %s \n", curr->am_dead, curr->group, curr->file_name);
        }
    }
}

//TODO:TEST IN MULTI THREADING AND ADD API REMOVE ETC
// #ifdef DEBUG
cach_hash_t *MY_CACHE;
icl_hash_t *FILES_STORAGE;
int main(int argc, char **argv){
    // cach_hash_t *ht = cach_hash_create(2, NULL);
    #define TEST 15000
    #define EXTRA 100000
    struct Cache_settings sett;
    sett.NUM_GROUP = 90;
    sett.CACHE_RANGE = 2;
    sett.START_INI_CACHE = TEST;
    sett.EXTRA_CACHE_SPACE = 0.049;
    FILES_STORAGE = NULL;
    MY_CACHE = NULL;

    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(sett.START_INI_CACHE, triple32, string_compare);
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
        if(ret_point.key) fprintf(fl_sto, "REMOVED: %s\n", (char*)ret_point.key);
        if(i == TEST-1){
            // fprintf(fl, "BEFORE-------------REMOVAL\n");
            // fprintf(fl_sto, "BEFORE-------------REMOVAL\n");
            // cach_hash_dump(fl, MY_CACHE);
            // icl_hash_dump(fl_sto, FILES_STORAGE);
        }
    }
    // fprintf(fl, "AFTER-------------REMOVAL\n");
    // fprintf(fl_sto, "AFTER-------------REMOVAL\n");
    // cach_hash_dump(fl, MY_CACHE);
    // icl_hash_dump(fl_sto, FILES_STORAGE);
    fclose(fl);
    fclose(fl_sto);
    // print_storage(FILES_STORAGE);
    // print_info(MY_CACHE);
    // printf("%d\n", MY_CACHE->nfiles);
    // cach_hash_destroy(MY_CACHE);
    // icl_hash_destroy(FILES_STORAGE, NULL, NULL);
}

// #endif // DEBUG