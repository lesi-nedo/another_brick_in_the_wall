/**
 * @file
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
    int empty;
    void* key;
    void *data;
    unsigned long int ref;
    int am_being_used;
    int am_being_removed;
    int need_to_be_removed;
    pthread_mutex_t wr_dl_ap_lck;
    struct icl_entry_s* next;
    //!KEEP ALL THE TIME UPDATED TWO TABLES
    cach_entry_t *me_but_in_cache;
    time_t time;
    struct icl_entry_s* prev;
};


struct icl_hash_s {
    int nbuckets;
    int nentries;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
};

extern icl_hash_t *FILES_STORAGE;

int
update_pos(cach_entry_t *);

icl_hash_t *
icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

icl_entry_t
* icl_hash_find(icl_hash_t *, void* );

icl_entry_t
* icl_hash_insert(icl_hash_t *, void*, void *);

int
icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*)),
    icl_hash_dump(FILE *, icl_hash_t *);

int icl_hash_delete_ext( icl_hash_t *, void*, void (*free_key)(void*), void (*free_data)(void*) );

/* compare function */
int 
string_compare(void* a, void* b);
unsigned int
triple32(void *);

int bind_two_tables_init_cache(cach_entry_t *, icl_entry_t *);
cach_entry_t  *bind_two_tables_create_entry(icl_entry_t *, int);
char *rand_string(char *, size_t);
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