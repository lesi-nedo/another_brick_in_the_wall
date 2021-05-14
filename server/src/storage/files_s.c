/**
 * @file icl_hash.c
 *
 * Dependency free hash table implementation.
 *
 * This simple hash table implementation should be easy to drop into
 * any other piece of code, it does not depend on anything else :-)
 * 
 * @author Jakub Kurzak
 */
/* $Id: icl_hash.c 2838 2011-11-22 04:25:02Z average $ */
/* $UTK_Copyright: $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "files_s.h"
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "../../includes/util.h"
#include "../../includes/thread_for_log.h"


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
/**
 * @copyright
 *This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
 *@author: Christopher Wellons 
 *@taken_from: https://github.com/skeeto/hash-prospector
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
unsigned int
triple32(void *arg)
{
    uint32_t x = *(uint32_t *)arg;
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    x *= 0x31848bab;
    x ^= x >> 14;
    return x;
}

int string_compare(void* a, void* b) 
{
    return (strcmp( (char*)a, (char*)b ) == 0);
}


/**
 * Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

icl_hash_t *
icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) )
{
    icl_hash_t *ht;
    int i;

    ht = (icl_hash_t*) malloc(sizeof(icl_hash_t));
    if(!ht) return NULL;

    ht->nentries = 0;
    ht->buckets = (icl_entry_t**)malloc(nbuckets * sizeof(icl_entry_t*));
    if(!ht->buckets) return NULL;

    ht->nbuckets = nbuckets;
    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : triple32;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    return ht;
}

/**
 * Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

icl_entry_t*
icl_hash_find(icl_hash_t *ht, void* key)
{
    icl_entry_t* curr;
    unsigned int hash_val;

    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( !curr->empty && ht->hash_key_compare(curr->key, key))
            return curr;

    return NULL;
}

/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */





icl_entry_t *
icl_hash_insert(icl_hash_t *ht, void* key, void *data)
{
    icl_entry_t *curr, *prev;
    unsigned int hash_val;
    int res;

    if(!ht || !key) return NULL;

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (prev = NULL, curr=ht->buckets[hash_val]; curr != NULL; prev = curr, curr=curr->next){
        if(curr->empty){
            //!LOCK ACQUIRED
            if(TRYLOCK(&curr->wr_dl_ap_lck)){
                if(curr->empty){
                    curr->key = key;
                    curr->data = data;
                    curr->empty = 0;
                    curr->need_to_be_removed = 0;
                    curr->am_being_removed = 0;
                    curr->ref = MY_CACHE->incr_entr;
                    curr->time = time(NULL);
                    curr->am_being_used = 1;
                    //!LOCK RELEASED if insert
                    UNLOCK(&curr->wr_dl_ap_lck);
                    cach_hash_insert_bind(MY_CACHE, curr);
                    return curr;
                }
            }
        }
        if ( ht->hash_key_compare(curr->key, key)){
            if(curr->am_being_removed){
                while(curr->am_being_removed){
                    #ifdef DEBUG
                    printf("%s\n",  (char *)curr->key);
                    #endif // DEBUG
                }
                if(TRYLOCK(&curr->wr_dl_ap_lck)){
                    //!LOCK ACQUIRED
                    if(curr->empty){
                        curr->key = key;
                        curr->data = data;
                        curr->empty = 0;
                        curr->am_being_removed = 0;
                        curr->need_to_be_removed = 0;
                        curr->ref = MY_CACHE->incr_entr;
                        curr->time = time(NULL);
                        curr->am_being_used = 1;
                        //!LOCK RELEASED if insert
                        UNLOCK(&curr->wr_dl_ap_lck);
                        cach_hash_insert_bind(MY_CACHE, curr);
                        return curr;
                    }
                }
            }
            errno = EEXIST;
            return(NULL); /* key already exists */
        }
    }
    /* if key was not found */
    curr = (icl_entry_t*)malloc(sizeof(icl_entry_t));
    if(!curr){ 
        return NULL;
    }

    curr->time = time(NULL);
    SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&(curr->wr_dl_ap_lck), NULL), "This was not expected, well it is a shame maybe try again.\n", NULL);
    curr->key = key;
    curr->ref= MY_CACHE->incr_entr;
    curr->data = data;
    curr->empty = 0;
    curr->am_being_removed = 0;
    curr->need_to_be_removed = 0;
    curr->prev = NULL;
    curr->next = NULL;
    long res1= 0, res2 = 0; 
    struct drand48_data buff;
    unsigned long seed = get_seed(pthread_self());
    srand48_r(seed, &buff);
    lrand48_r(&buff, &res1);
    lrand48_r(&buff, &res2);
    if(prev == NULL || (res1 - res2) < 0){
        curr->next = ht->buckets[hash_val]; /* add at start */
        if(ht->buckets[hash_val] != NULL) ht->buckets[hash_val]->prev = curr;
        ht->buckets[hash_val] = curr;
        ht->nentries++;
        curr->am_being_used = 1;
        cach_hash_insert_bind(MY_CACHE, curr);
        return curr;

    } else {
        prev->next = curr;
        curr->prev = prev;
        curr->am_being_used = 0;
        cach_hash_insert_bind(MY_CACHE, curr);
        return curr;
    }
    
}

/**
 * Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete_ext(icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*))
{
    icl_entry_t *curr;
    unsigned int hash_val;
    if(!ht || !key) {
        errno = EINVAL;
        return -1;
    }
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    for (curr=ht->buckets[hash_val]; curr != NULL; )  {
        if ( ht->hash_key_compare(curr->key, key)) {
            //!LOCK ACQUIRED
            LOCK(&curr->wr_dl_ap_lck);
            curr->need_to_be_removed = 1;
            curr->am_being_removed = 1;
            if(curr->am_being_used){
                errno = ETXTBSY;
                //!LOCK RELEASED if am_being_used == 1
                UNLOCK(&curr->wr_dl_ap_lck);
                return -1;
            }
           //This might be problematic
            for(;;){
                if(!curr->am_being_removed){
                    if((*free_key) && curr->key) free_key(curr->key);
                    if((*free_data) && curr->data) free_key(curr->data);
                    break;
                }
                if(!TRYLOCK(&curr->me_but_in_cache->mutex)){
                    //!LOCK ACQUIRED
                    if(curr->key == curr->me_but_in_cache->file_name){
                        #ifdef DEBUG
                            assert(curr->ref == *curr->me_but_in_cache->ref);
                        #endif // DEBUG
                        curr->me_but_in_cache->am_dead =0;
                        curr->me_but_in_cache->file_name = NULL;
                        if((*free_key) && curr->key) free_key(curr->key);
                        if((*free_data) && curr->data) free_key(curr->data);
                        //!LOCK RELEASED
                        UNLOCK(&curr->me_but_in_cache->mutex);
                        break;
                    }
                    #ifdef DEBUG
                        printf("IF CODE IS BUGGED THEN THIS LOOP IN INFINITE. (icl_hash_delete_ext)\n");
                    #endif // DEBUG
                    //!LOCK RELEASED
                    UNLOCK(&curr->me_but_in_cache->mutex);
                }
            }
            curr->empty = 1;
            curr->ref = 0;
            curr->am_being_used = 0;
            curr->time = 0;
            curr->am_being_removed = 0;
            //!LOCK RELEASED
            UNLOCK(&ht->buckets[hash_val]->wr_dl_ap_lck);
            return 0;
        }
        curr = curr->next;
    }
    return -1;
}

/**
 * Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int
icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void*), void (*free_data)(void*))
{
    icl_entry_t *bucket, *curr, *next;
    int i;

    if(!ht) return -1;

    for (i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr=bucket; curr!=NULL; ) {
            next=curr->next;
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            if(pthread_mutex_destroy(&curr->wr_dl_ap_lck)){
                fprintf(stderr, "trying to destroy the lock.\n");
                //dprintf(ARG_LOG_TH.pipe[WRITE], "ERROR: trying to destroy lock in icl_hash_destroy.\n");
            }
            free(curr);

            curr=next;
        }
    }

    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);

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
icl_hash_dump(FILE* stream, icl_hash_t* ht)
{
    icl_entry_t *bucket, *curr;
    int i;

    if(!ht) return -1;

    for(i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key)
                fprintf(stream, "icl_hash_dump: %s: %p\n", (char *)curr->key, curr->data);
            curr=curr->next;
        }
    }

    return 0;
}


int bind_two_tables_init_cache(cach_entry_t *entry, icl_entry_t *node){
    if(node == NULL || entry  == NULL){
        errno=EINVAL;
        return -1;
    }
    entry->am_dead = 0;
    entry->ref = &node->ref;
    entry->file_name = node->key;
    node->me_but_in_cache = entry;
    return 0;
}

cach_entry_t *bind_two_tables_create_entry(icl_entry_t *node, int group){
    if(node == NULL){
        errno=EINVAL;
        return NULL;
    }
    int res = 0;
    cach_entry_t *entry = (cach_entry_t *) malloc(sizeof(cach_entry_t));
    CHECK_EQ_EXIT(malloc, entry, NULL, "Sorry not sorry but you are out of memory, bye.\n", NULL);
    SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&entry->mutex, NULL), "Sorry not sorry but you something went terribly wrong, bye.\n", NULL);
    entry->am_dead = 0;
    entry->ref = &node->ref;
    entry->group = group;
    entry->next = NULL;
    entry->prev = NULL;
    entry->file_name = node->key;
    node->me_but_in_cache = entry;
    return entry;
}

char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = lrand48() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

void print_storage(icl_hash_t *ht){
    for(int i=0; i< ht->nbuckets; i++){
        icl_entry_t *curr;
        if(ht->buckets[i] != NULL) {
        for (curr=ht->buckets[i]; curr != NULL;){
            if(!curr->empty){
                printf("%s->", (char *)curr->key);
                curr=curr->next;
            }
        }
        }else printf("NULL");
        printf("\n");

    }
}

#ifdef DEBUG_FILES

void print_rev(icl_hash_t *ht){
    printf("----------------------\n");
    for(size_t i=0; i<ht->nbuckets; i++){
        icl_entry_t *curr = NULL;
        icl_entry_t *prev=NULL;
        for(curr=ht->buckets[i]; curr != NULL; ){
        printf(":D");

            prev = curr;
            curr = curr->next;
        }
        for(; prev != NULL;){
            printf("%s->",(char *) prev->key);
            prev = prev->prev;
        }
        printf("\n");
    }
}

void print_delte_rand(icl_hash_t *ht){
    srand(time(NULL));
    printf(":DDD");
    printf("----------------------\n");
    icl_entry_t *curr = NULL;
    for(size_t i=0; i<ht->nbuckets; i++){
        if(ht->buckets[i + rand() % (ht->nbuckets - i)]){
            for(curr=ht->buckets[i + rand() % (ht->nbuckets - i)]; curr != NULL; ){
                if(curr->next == NULL){
                    icl_hash_delete_ext(ht, curr->key, NULL, NULL);
                    break;
                } else {
                    if(((rand() % 5) ^ 3) ) continue;
                    icl_hash_delete_ext(ht, curr->next->key, NULL, NULL);
                }
                curr = curr->next;
            }
            
        }
    }
}





int main(int argc, char **argv){
    icl_hash_t *ht = icl_hash_create(40, triple32, NULL);
    char rand_t[30][5];
    srand(time(NULL));
    srand48(~((pthread_self() ^ (2342323522  | getpid())) << 22));
    for(size_t i=0; i <30; i++){
        rand_string(rand_t[i], 5);
        icl_hash_insert(ht, (void *) &(rand_t[i]), NULL);
    }
    // for(size_t i=0; i <50; i++){
    //     printf("-%s-", rand_t[i]);
    // }
    print(ht);
    print_delte_rand(ht);
    print(ht);
    print_delte_rand(ht);
    // icl_hash_insert(ht, rand_t[rand() % 30], NULL);
    // print(ht);

    icl_hash_destroy(ht, NULL, NULL);
}
#endif // DEBUG


