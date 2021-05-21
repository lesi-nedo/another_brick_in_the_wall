#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "../../../includes/new.h"
#include "../../../includes/util.h"


/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

//! I DID NOT TAKE IN CONSIDERATION THAT SOME OF THIS VALUE MIGHT CRASH THE
//! PROGRAM, SO IF YOU WANT TO CHANGE IT FEEL FREE BUT THE PROGRAM MIGHT FAIL.
#define MOD_SLEEP 1 //this is gets the range which will be used in sleep i.e random_number % MOD_SLEEP
#define NUM_MIN_STR 1 //this number is a lower bounder for how many value will be inserted
#define NUM_MAX_STR 10 //this number is a upper bounder for how many value will be inserted
#define LENGTH_RAND_STR 5 //This is the length of the random string to be inserted
#define TEST 34 //HOW MANY FILES
#define EXTRA 2000 // This value for the main thread, it specifying how many extra strings will be inserted i.e TEST+EXTRA
#define NUM_THREADS 9 //number of running threads
#define MAIN_THREAD_SLEEP 20 //for how long main thread will go to sleep before calling pthread_kill with signal SIGQUIT
#define INCREMENT_THIS 10000 //this value is for the library timeout
#define NUM_GROUP_M 10 //this is for number of levels in cache
#define CACHE_RANGE_M 4 //this is the range in which will be inserted entries in a specific level
#define EXTRA_CACHE_SPACE_M 0.09 //extra space
#define INSERT_EXTRA 20 //this value is for infinite loop in which threads will be executing inserts, search and deletes
#define clear() printf("\033[H\033[J")
#define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))
static volatile sig_atomic_t sig = 0;
icl_hash_t *FILES_STORAGE = NULL;
cach_hash_t *MY_CACHE = NULL;
pthread_mutex_t TO_FREE_LCK = PTHREAD_MUTEX_INITIALIZER; 


char **create_matr(size_t row, size_t col){
    char **to_ret = (char **)malloc(row*sizeof(char *));
    if(to_ret == NULL){
        perror("create_matr");
        pthread_exit(NULL);
    }
    for(size_t i = 0; i < row; i++){
        to_ret[i] = (char *)calloc(col, sizeof(char));
    }
    return to_ret;
}

struct thr_args {
    unsigned int sync_thr;
    FILE *fl_stor;
    FILE *fl_ca;
    FILE *op;
    FILE *all_deletes;
    FILE *vict;
    FILE *not_del;
    icl_hash_t *stor;
    cach_hash_t *cach;
    char ***to_free;
    size_t size1;

};

static void sig_hand(int sign){
    sleep(1);
    if(sign == SIGQUIT){
        sig = 1;
    }
}


void* isrt_rand(void *args_th){
    int res = 2;
    //SIGNAL MASK
    sigset_t mask, old_m;
    sigemptyset(&mask);
    sigaddset(&mask, SIGQUIT);
    SYSCALL_EXIT(sigprocmask, res,  sigprocmask(SIG_BLOCK, &mask, &old_m), "Nothing to do here, I'm out, \033[1;31mBye.\033[0;37m\n", NULL);
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_hand;
    SYSCALL_EXIT(sigaction, res, sigaction(SIGQUIT, &act, NULL), "I don't know what to say, bro probably better quitting. Bye\n", NULL);


    SYSCALL_EXIT(sigprocmask, res,  sigprocmask(SIG_SETMASK, &old_m, NULL), "Nothing to do here, I'm out, \033[1;31mBye.\033[0;37m\n", NULL);
    struct thr_args *args = (struct thr_args *)args_th;
    sleep(args->sync_thr);
    int j = 0;
    pointers ret;
    memset(&ret, 0, sizeof(pointers));
    // unsigned int rand_sleep = get_rand() % MOD_SLEEP;
    unsigned int SIZE_MA = NUM_MIN_STR + get_rand() % (NUM_MAX_STR - NUM_MIN_STR +1);
    char **rand_t = create_matr(SIZE_MA+1, LENGTH_RAND_STR);
    for(size_t i = 0; i < SIZE_MA; i++){
        printf("\033[1A\033[1;33m THREAD: %zd I'M Still Running-%zd\033[0;37m\033[s\n", pthread_self(), i);
        rand_string(rand_t[i], LENGTH_RAND_STR);
        //sleep(rand_sleep);
        // sleep(1);
        fprintf(args->op, "%s\n", rand_t[i]);
        icl_hash_insert(args->stor, rand_t[i], NULL, &ret);
        if(ret.key) fprintf(args->vict, "%s\n", (char *)ret.key);
        // rand_sleep = get_rand() % MOD_SLEEP;
        if((get_rand() % (i+1)) > ((int) TEST % (i+1))){
            char *rand_str_del = rand_t[get_rand() % (i+1)];
            // sleep(rand_sleep);
            errno = 0;
            res = icl_hash_delete_ext(args->stor, rand_str_del, NULL, NULL, &ret);
            if(res == 0) fprintf(args->all_deletes, "%s\n", rand_str_del);
            else fprintf(args->not_del, "%s\n", rand_str_del);
            // rand_sleep = get_rand() % MOD_SLEEP;
        }
        if((get_rand() % (i+1)) - (get_rand() % (i+1))>0){
            char *rand_str_find = rand_t[get_rand() % (i+1)];
            icl_hash_find(FILES_STORAGE, rand_str_find);
        }
        // sleep(rand_sleep);
        fflush(args->op);
    }
    *rand_t[SIZE_MA]='\0';
    char **to_ins = create_matr(INSERT_EXTRA, LENGTH_RAND_STR);
    int si =0;
    for(;sig==0;){
        j++;
        printf("\033[1A\033[1;33mTHREAD: %zd  I'M Still Running-%d\033[0;37m\033[s\n", pthread_self(), j);
        fflush(stdout);
        // rand_sleep = get_rand() % MOD_SLEEP+3;
        if((get_rand() % (j+1)) - (get_rand() % (j+1))>0){
            // sleep(rand_sleep);
            char *rand_str_find = rand_t[get_rand() % SIZE_MA];
            icl_hash_find(FILES_STORAGE, &*rand_str_find);
            sched_yield();
        }
        if(j < INSERT_EXTRA){
            if((get_rand() ^ get_rand()) - (get_rand() ^ get_rand()) > 0){
                // sleep(rand_sleep);
                    rand_string(to_ins[si], LENGTH_RAND_STR);
                    fprintf(args->op, "%s\n", to_ins[si]);
                    icl_hash_insert(FILES_STORAGE, to_ins[si], NULL, &ret);
                    if(ret.key) fprintf(args->vict, "%s\n", (char *)ret.key);
                    si++;
            }
            sched_yield();
        }
        // rand_sleep = get_rand() % MOD_SLEEP;
        // sleep(rand_sleep);
    }
    struct tm r_tm;
    time_t now = time(NULL);
    struct tm lc_tm = *localtime_r(&now, &r_tm);
    fprintf(args->fl_ca, "THREAD: %zd  HAS FINISHED ---------------------AT: %d-%02d-%02d  %02d:%02d------------------------->\n",pthread_self(), lc_tm.tm_year+1900, lc_tm.tm_mon+1, lc_tm.tm_mday, lc_tm.tm_hour, lc_tm.tm_min);
    LOCK(&TO_FREE_LCK);
    args->to_free[args->size1++]=rand_t;
    args->to_free[args->size1++]=to_ins;
    UNLOCK(&TO_FREE_LCK);
    return NULL;
}

START_TEST(test_parse){
    int res = 0;
    int SIZE_M = EXTRA+TEST < 0 ? TEST: EXTRA+TEST;
    struct Cache_settings sett;
    sett.NUM_GROUP = NUM_GROUP_M;
    sett.CACHE_RANGE = CACHE_RANGE_M;
    sett.START_INI_CACHE = TEST;
    sett.EXTRA_CACHE_SPACE = EXTRA_CACHE_SPACE_M;

    pointers ret_point;
    pointers ret_del;
    memset(&ret_point, 0, sizeof(ret_point));
    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(sett.START_INI_CACHE, hash_pjw, string_compare);
    char **rand_t = create_matr(SIZE_M, LENGTH_RAND_STR);
    FILE *fl = fopen("cach.txt", "a+");
    FILE *fl_sto = fopen("storage.txt", "a+");
    FILE *all_deletes = fopen("all_delets.txt", "a");
    FILE *not_del = fopen("not_delets.txt", "a");
    FILE *vict = fopen("victims.txt", "a");
    FILE *op = fopen("all_inserts.txt", "a+");
    CHECK_EQ_EXIT(fopen, op, NULL, "SEE YAAA.\n", NULL);
    CHECK_EQ_EXIT(fopen, vict, NULL, "SEE YAAA.\n", NULL);
    CHECK_EQ_EXIT(fopen, fl, NULL, "SEE YAAA.\n", NULL);
    CHECK_EQ_EXIT(fopen, fl_sto, NULL, "SEE YAAA.\n", NULL);
    CHECK_EQ_EXIT(fopen, all_deletes, NULL, "SEE YAAA.\n", NULL);
    CHECK_EQ_EXIT(fopen, not_del, NULL, "SEE YAAA.\n", NULL);

    pthread_t threads[NUM_THREADS];
    struct thr_args args;
    memset(&args, 0, sizeof(args));
    args.sync_thr = NUM_THREADS;
    args.fl_ca = fl;
    args.fl_stor = fl_sto;
    args.stor = FILES_STORAGE;
    args.cach = MY_CACHE;
    args.not_del = not_del;
    args.all_deletes = all_deletes;
    args.vict = vict;
    args.op = op;
    args.to_free = (char ***)malloc(sizeof(char **)*((2*NUM_THREADS)+1));
    printf("\033[1;31mSTART\033[0;37m\n");
    for(size_t i = 0; i < NUM_THREADS; i++){
        SYSCALL_EXIT(pthread_create, res, pthread_create(&threads[i], NULL, isrt_rand, (void *)&args), "I'm embarrassed. Bye\n", NULL);
        args.sync_thr = NUM_THREADS-i;

    }
    
    for(size_t i=0; i < SIZE_M; i++){
        printf("\033[1A\033[1;32mMAIN##: %zd I'M Still Running-%zd\033[0;37m\033[s\n", pthread_self(), i);
        fflush(stdout);
        
        // sleep(ran_s+2);
        rand_string(rand_t[i], LENGTH_RAND_STR);
        fprintf(op, "%s\n", rand_t[i]);
        if((icl_hash_insert(FILES_STORAGE, (void *)rand_t[i], NULL, &ret_point), errno = 0) != 0 && errno != EBUSY){
            fprintf(stderr, "Error fatal in icl_hash_insert\n");
            pthread_exit(NULL);
        }
        if(ret_point.key) fprintf(vict, "%s\n", (char *)ret_point.key);
        if(i > (int) (TEST % (i+1))/2 && i % 2 == 1){
            char *rand_rem = rand_t[lrand48() % (i+1)];
            res = icl_hash_delete_ext(FILES_STORAGE, rand_rem, NULL, NULL, &ret_del);
            if(res == 0) fprintf(all_deletes, "%s\n", rand_rem);
            else fprintf(not_del, "%s\n", rand_rem);
            if(res == -1 && errno == ETXTBSY) fprintf(fl_sto, "FILE BUSY: %s -------\n", rand_rem);
        }
    }
    int allready_sended[NUM_THREADS], i =0;
    for(size_t i = 1; i < NUM_THREADS; i++) allready_sended[i] = -1;
    allready_sended[0] = get_rand() % NUM_THREADS;
    printf("\033[1A\033[50C\033[1;32mMAIN THREAD:%zd GOES TO SLEEP\033[0;37m\033[s\n", pthread_self());
    sleep(MAIN_THREAD_SLEEP);
    while(1){
        int rand_th = get_rand() % NUM_THREADS;
        while(i < NUM_THREADS){
            if(allready_sended[i]!= -1 && allready_sended[i] == rand_th) break;
            if(allready_sended[i] == -1){
                allready_sended[i] = rand_th;
                break;
            }
            i++;
        }
        if(i == NUM_THREADS-1) break;
        i = 0;
    }
    for(size_t i = 0; i<NUM_THREADS; i++){
        SYSCALL_EXIT(pthread_kill, res, pthread_kill(threads[allready_sended[i]], SIGQUIT), "What a shame!\n", NULL);
    
    }
    printf("\033[1A\033[1;93mBye. :)\033[K\033[0;37m\n");
    for(size_t i = 0; i<NUM_THREADS; i++){
        SYSCALL_EXIT(pthread_join, res, pthread_join(threads[i], NULL), "Who would have thought.\n", NULL);
    }
    fflush(stdout);
    fflush(fl_sto);
    fflush(fl);
    fflush(op);
    cach_hash_dump(fl, MY_CACHE);
    icl_hash_dump(fl_sto, FILES_STORAGE);
    fclose(fl);
    fclose(fl_sto);
    fclose(op);
    fclose(all_deletes);
    fclose(not_del);
    fclose(vict);
    // print_storage(FILES_STORAGE);
    for(size_t i = 0; i <MY_CACHE->NUM_GROUP; i++){
        cach_entry_t *curr = NULL;
        int dead = 0, not_dead = 0;
        ck_assert_int_ge(MY_CACHE->buckets[i]->num_dead, 0);
        ck_assert_int_le(0, MY_CACHE->buckets[i]->nfiles_row);
        for(curr = MY_CACHE->buckets[i]->head; curr != NULL; curr = curr->next){
            if(curr->am_dead) dead++;
            else{ 
                not_dead++;
                icl_entry_t *found = icl_hash_find(FILES_STORAGE, curr->file_name);
                ck_assert_ptr_nonnull(found);
                ck_assert_ptr_eq(curr->ref, &found->ref);
                ck_assert_int_eq(curr->group, MY_CACHE->buckets[i]->group);
                ck_assert_str_eq(curr->me_but_in_store->key, curr->file_name);
                ck_assert_str_eq(curr->me_but_in_store->me_but_in_cache->file_name, curr->file_name);
            }
        }
        ck_assert_int_eq(MY_CACHE->buckets[i]->num_dead, dead);
        ck_assert_int_ge(MY_CACHE->buckets[i]->nfiles_row, not_dead);
        ck_assert_int_eq(MY_CACHE->nfiles, FILES_STORAGE->nentries);

    }
    int empty = 0;
    int not_empt = 0;
    for(size_t i = 0; i < FILES_STORAGE->nbuckets; i++){
        icl_entry_t *curr = NULL;
        for(curr = FILES_STORAGE->buckets[i]; curr != NULL; curr = curr->next){
            if(curr->empty) empty++;
            else not_empt++;
            if(!curr->empty && curr->me_but_in_cache) ck_assert_str_eq(curr->key, curr->me_but_in_cache->file_name);
            else if(!curr->empty && curr->me_but_in_cache) ck_assert_int_gt(curr->empty, 5);
            if(!curr->empty && curr->me_but_in_cache) ck_assert_str_eq(curr->key, curr->me_but_in_cache->me_but_in_store->key);
            else if(!curr->empty && curr->me_but_in_cache) ck_assert_int_gt(curr->empty, 5);
        }
    }
    ck_assert_int_gt(FILES_STORAGE->nentries+empty, 0);
    for(size_t i = 0; i< args.size1; i++){
        int IND =0;
        if(i % 2 == 1){
            for(size_t j = 0; j < INSERT_EXTRA; j++){
                free(args.to_free[i][j]);
            }
        } else {
            while(*args.to_free[i][IND]!= '\0'){
                free(args.to_free[i][IND]);
                IND++;
            }
            free(args.to_free[i][IND]);
        }
        free(args.to_free[i]);
    }
    for(size_t i = 0; i< SIZE_M; i++){
        free(rand_t[i]);
    }
    free(rand_t);
    free(args.to_free);
    cach_hash_destroy(MY_CACHE);
    icl_hash_destroy(FILES_STORAGE, NULL, NULL);
    pthread_mutex_destroy(&TO_FREE_LCK);
} END_TEST

Suite *parse_suite(void){
    Suite *s;
    TCase *tc_core;

    s=suite_create("Parse");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_parse);
    tcase_set_timeout(tc_core, MAIN_THREAD_SLEEP*INCREMENT_THIS);
    suite_add_tcase(s, tc_core);
    return s;
}

int main (void){
    int no_failed = 0;
    Suite *s;
    SRunner *runner;
    

    s= parse_suite();
    runner=srunner_create(s);
    srunner_set_xml(runner, "test_store");
    srunner_run_all(runner, CK_NORMAL);
    no_failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (no_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}