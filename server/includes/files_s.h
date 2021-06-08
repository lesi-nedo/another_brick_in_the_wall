/**
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

#ifndef FILES_S_H
#define FILES_S_H

typedef struct icl_entry_s icl_entry_t;
typedef struct icl_hash_s icl_hash_t;

#include "new.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

struct icl_entry_s {
    unsigned long int empty;
    void* key;
    void *data;
    unsigned long int ref;
    int been_modified;
    size_t OWNER;
    int O_LOCK;
    size_t ptr_tail;
    int open;
    long int am_being_used;
    pthread_mutex_t wr_dl_ap_lck;
    struct icl_entry_s* next;
    //!KEEP ALL THE TIME UPDATED TWO TABLES
    cach_entry_t *me_but_in_cache;
    time_t time;
    struct icl_entry_s* prev;
};


struct icl_hash_s {
    int nbuckets;
    pthread_mutex_t stat_lck;
    long long int MAX_SPACE_AVAILABLE;
    long int nentries;
    long int max_files;
    long int total_victims;
    long long int total_bytes;
    long long int max_bytes;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
};

extern icl_hash_t *FILES_STORAGE;
icl_hash_t *
icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

icl_entry_t
* icl_hash_find(icl_hash_t *, void* );

icl_entry_t
* icl_hash_insert(icl_hash_t *, void*, void *, size_t, pointers *);

int
icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*)),
    icl_hash_dump(FILE *, icl_hash_t *);

int icl_hash_delete_ext( icl_hash_t *, void*, pointers *, size_t );


/* compare function */
int 
string_compare(void* a, void* b);

unsigned int
hash_pjw(void*);

cach_entry_t  *bind_two_tables_create_entry(icl_entry_t *, int);
void print_storage(icl_hash_t *);

#define icl_hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* icl_hash_h */