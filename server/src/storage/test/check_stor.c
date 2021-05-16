
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "../../../includes/new.h"

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


START_TEST(test_parse){
    cach_hash_t *MY_CACHE;
    icl_hash_t *FILES_STORAGE;
    #define TEST 1231
    struct Cache_settings sett;
    sett.NUM_GROUP = 10;
    sett.CACHE_RANGE = 5;
    sett.START_INI_CACHE = TEST;
    sett.EXTRA_CACHE_SPACE = 0.049;

    MY_CACHE = cach_hash_create(sett, get_next_index);
    FILES_STORAGE = icl_hash_create(sett.START_INI_CACHE, triple32, string_compare);
    char rand_t[TEST][5];
    srand(time(NULL));
    srand48(~((pthread_self() ^ (2342323522  | getpid())) << 22));
    for(size_t i=0; i <TEST; i++){
        rand_string(rand_t[i], 5);
        icl_hash_insert(FILES_STORAGE, (void *) &(rand_t[i]), NULL, NULL);
    }

    cach_hash_destroy(MY_CACHE);
    icl_hash_destroy(FILES_STORAGE, NULL, NULL);
} END_TEST

Suite *parse_suite(void){
    Suite *s;
    TCase *tc_core;

    s=suite_create("Parse");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_parse);
    suite_add_tcase(s, tc_core);
    return s;
}

int main (void){
    int no_failed = 0;
    Suite *s;
    SRunner *runner;

    s= parse_suite();
    runner=srunner_create(s);
    srunner_run_all(runner, CK_NORMAL);
    no_failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (no_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}