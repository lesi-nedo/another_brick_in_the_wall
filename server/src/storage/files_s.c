#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "../../includes/files_s.h"
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "../../includes/util.h"
#include "../../includes/thread_for_log.h"
#define START_SIZE_CALLOC 10


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberg's (PJW) generic hashing
 * algorithm based on Allen Honolulu's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
unsigned int
hash_pjw(void* key)
{
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}
int string_compare(void* a, void* b) 
{
    if(a == NULL || b == NULL){ 
        return 0;
    }
    return (strcmp( (char*)a, (char*)b ) == 0);
}

int msleep(long miliseconds)
{
   struct timespec req, rem;

   if(miliseconds > 999)
   {   
        req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
        req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
   }   
   else
   {   
        req.tv_sec = 0;                         /* Must be Non-Negative */
        req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
   }   

   return nanosleep(&req , &rem);
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
    for(i=0;i<ht->nbuckets;i++){
        ht->buckets[i] = (icl_entry_t*)calloc(1,sizeof(icl_entry_t));
        if(!ht->buckets[i]){ 
            return NULL;
        }
        int res = 0;
        ht->buckets[i]->time = 0;
        SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&(ht->buckets[i]->wr_dl_ap_lck), NULL), "This was not expected, well it is a shame maybe try again.\n", NULL);
        ht->buckets[i]->key = NULL;
        ht->buckets[i]->ref= 0;
        ht->buckets[i]->data = NULL;
        ht->buckets[i]->empty = 1;
        ht->buckets[i]->me_but_in_cache = NULL;
        ht->buckets[i]->prev = NULL;
        ht->buckets[i]->next = NULL;
    }

    ht->hash_function = hash_function ? hash_function : hash_pjw;
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

    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next){
        if (!curr->empty && ht->hash_key_compare(curr->key, key)){
            curr->ref = curr->ref + 2;
            return curr;
        }
    }
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
icl_hash_insert(icl_hash_t *ht, void* key, void *data, pointers *ret)
{
    icl_entry_t *curr, *prev, *empty = NULL;
    pointers ret2;
    memset(&ret2, 0, sizeof(pointers));
    unsigned int hash_val;
    pthread_mutex_t *THIS_IS_A_LCK =NULL;
    int res;

    if(!ht || !key || !ret){
        errno = EINVAL;
        return NULL;
    }
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    THIS_IS_A_LCK = &ht->buckets[hash_val]->wr_dl_ap_lck;
    //!LOCK ACQUIRED
    LOCK(THIS_IS_A_LCK);
    for (prev = NULL, curr=ht->buckets[hash_val]; curr != NULL; prev = curr, curr=curr->next){
        if (ht->hash_key_compare(curr->key, key)){
            errno = EEXIST;
            //!LOCK RELEASED
            UNLOCK(THIS_IS_A_LCK);
            return(NULL); /* key already exists */
        } else if(curr->empty){
            empty=curr;

        }
    }
    if(empty){
        empty->empty = 0;
        ht->nentries++;
        empty->key = key;
        empty->data = data;
        empty->me_but_in_cache = NULL;
        empty->ref = MY_CACHE->incr_entr;
        empty->time = time(NULL);
        //!LOCK RELEASED
        UNLOCK(THIS_IS_A_LCK);
        cach_hash_insert_bind(MY_CACHE, empty, ret);
        return curr;
    }
    /* if key was not found */
    curr = (icl_entry_t*)malloc(sizeof(icl_entry_t));
    if(!curr){
        return NULL;
    }

    curr->time = time(NULL);
    curr->empty = 0;
    SYSCALL_EXIT(pthread_mutex_init, res,  pthread_mutex_init(&(curr->wr_dl_ap_lck), NULL), "This was not expected, well it is a shame maybe try again.\n", NULL);
    curr->key = key;
    curr->ref= MY_CACHE->incr_entr;
    curr->data = data;
    curr->am_being_used = 0;
    curr->me_but_in_cache = NULL;
    curr->prev = NULL;
    curr->next = NULL;
    ht->nentries++;
    //add element at the tail
    prev->next = curr;
    curr->prev = prev;
    //!LOCK RELEASED
    UNLOCK(THIS_IS_A_LCK);
    cach_hash_insert_bind(MY_CACHE, curr, ret);
    return curr;
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
 *///TODO: CHANGE THE WAY YOUDELETE AN ELEMENT FROM TABLE
int icl_hash_delete_ext(icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*), pointers *ret)
{
    icl_entry_t *curr;
    unsigned int hash_val;
    if(!ht || !key) {
        errno = EINVAL;
        return -1;
    }
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    for (curr=ht->buckets[hash_val]; curr != NULL;  curr = curr->next)  {
        if (!curr->empty && ht->hash_key_compare(curr->key, key)) {
            //!LOCK ACQUIRED
            LOCK(&curr->wr_dl_ap_lck);
            if(curr->am_being_used){
                errno = ETXTBSY;
                 //!LOCK RELEASED
                UNLOCK(&curr->wr_dl_ap_lck);
                return -1;
            }
            curr->empty = 1;
            void *me_to = curr->key;
            ret->key = curr->key;
            ret->data = curr->data;
            cach_entry_t *in_cach = curr->me_but_in_cache;
            curr->key = NULL;
            curr->data = NULL;
            curr->me_but_in_cache = NULL;
            ht->nentries--;
            curr->ref = 0;
            curr->time = 0;
            //!LOCK RELEASED
            UNLOCK(&curr->wr_dl_ap_lck);
            //This might be problematic
            if(in_cach){
                LOCK(&in_cach->mutex)
                //!LOCK ACQUIRED
                if(in_cach->am_dead == 0){
                    if(string_compare(me_to, in_cach->file_name)){
                        in_cach->am_dead =1;
                        in_cach->file_name = NULL;
                        MY_CACHE->buckets[in_cach->group]->num_dead++;
                        MY_CACHE->buckets[in_cach->group]->nfiles_row--;
                        in_cach->me_but_in_store = NULL;
                        MY_CACHE->nfiles--;
                        //!LOCK RELEASED
                        UNLOCK(&in_cach->mutex);
                        return 0;
                    }
                }
                UNLOCK(&in_cach->mutex);
            }
            return 0;
        }
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
            if((errno = pthread_mutex_destroy(&curr->wr_dl_ap_lck))){
                fprintf(stderr, "trying to destroy the lock. ERROR: %s\n", strerror(errno));
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
        for(curr=bucket; curr!=NULL; curr=curr->next) {
            if(!curr->empty && curr->key){
                if(curr->empty && (curr->me_but_in_cache == NULL || curr->me_but_in_cache->am_dead)) curr->me_but_in_cache = NULL;
                fprintf(stream, "STORAGE: %s ", (char *)curr->key);
                if(curr->me_but_in_cache) fprintf(stream, "CACHE: %s\n", curr->me_but_in_cache->file_name);
                else fprintf(stream, "CACHE: (null)\n");
            }
        }
    }
    // fprintf(stream, "SUMMARY: nbuckets: %d nentries: %d\n", ht->nbuckets, ht->nentries);
    return 0;
}



cach_entry_t *bind_two_tables_create_entry(icl_entry_t *node, int group){
    if(node == NULL){
        errno=EINVAL;
        return NULL;
    }
    cach_entry_t *entry = (cach_entry_t *) malloc(sizeof(cach_entry_t));
    //-1 indicates that is a temporary node
    entry->am_dead = -1;
    entry->ref = &node->ref;
    entry->group = group;
    entry->next = NULL;
    entry->prev = NULL;
    entry->file_name = node->key;
    entry->me_but_in_store = node;
    node->me_but_in_cache = entry;
    entry->time = node->time;
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
            printf("%s->", (char *)curr->key);
            curr=curr->next;
            }
            printf("\n");
        } else printf("NULL\n");

    }
}


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
    icl_entry_t *curr = NULL;
    pointers ret;
    for(size_t i=0; i<ht->nbuckets; i++){
        if(ht->buckets[i + rand() % (ht->nbuckets - i)]){
            for(curr=ht->buckets[i + rand() % (ht->nbuckets - i)]; curr != NULL; ){
                if(curr->next == NULL){
                    icl_hash_delete_ext(ht, curr->key, NULL, NULL, &ret);
                    break;
                } else {
                    if(((rand() % 5) ^ 3) ) continue;
                    icl_hash_delete_ext(ht, curr->next->key, NULL, NULL, &ret);
                }
                curr = curr->next;
            }
        }
    }
}




#ifdef DEBUG_NEW
int main(int argc, char **argv){
    FILES_STORAGE = icl_hash_create(40, hash_pjw, NULL);
    struct Cache_settings sett;
    sett.NUM_GROUP = 7;
    sett.CACHE_RANGE = 3;
    sett.START_INI_CACHE = 10;
    sett.EXTRA_CACHE_SPACE = 0.049;
    FILES_STORAGE = NULL;
    MY_CACHE = NULL;

    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(20, hash_pjw, string_compare);
    char rand_t[30][5];
    srand(time(NULL));
    srand48(~((pthread_self() ^ (2342323522  | getpid())) << 22));

    for(size_t i=0; i <30; i++){
        pointers ret;
        rand_string(rand_t[i], 5);
        icl_hash_insert(FILES_STORAGE, (void *) &(rand_t[i]), NULL, &ret);
    }
    // for(size_t i=0; i <50; i++){
    //     printf("-%s-", rand_t[i]);
    // }
    print_storage(FILES_STORAGE);
    print_delte_rand(FILES_STORAGE);
    print_storage(FILES_STORAGE);
    print_delte_rand(FILES_STORAGE);
    // icl_hash_insert(ht, rand_t[rand() % 30], NULL);
    // print(ht);

    icl_hash_destroy(FILES_STORAGE, NULL, NULL);
}
#endif // DEBUG


