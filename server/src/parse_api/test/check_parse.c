
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "../../../includes/parsing_conf.h"

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
    char str[] = {"./test.txt"};
    char str2[] = {"./test2.txt"};
    Server_conf settings[num_avail_settings];
    init_serv_conf(settings);
    init_settings_arr(settings);
    parse_file(str, settings);
    for(int i = 0; i < num_avail_settings; i++){
        unsigned int hash_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        ck_assert_str_eq((char *)available_settings[i], (char *)settings[hash_val].setting);
        if(!settings[i].str_or_int){
            ck_assert(settings[i].value_string);
            free(settings[i].value_string);
        } else if(settings[i].str_or_int == 1) ck_assert(settings[i].value);
        else ck_assert(settings[i].value_float);
    }
    parse_file(str2, settings);
    for(int i = 0; i < num_avail_settings; i++){
        unsigned int hash_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        ck_assert_str_eq((char *)available_settings[i], (char *)settings[hash_val].setting);
        if(!settings[i].str_or_int){
            fflush(stdout);
            ck_assert(settings[i].value_string);
            free(settings[i].value_string);
        } else if(settings[i].str_or_int == 1){
            fflush(stdout);
            ck_assert(settings[i].value);
        }
        else{
            fflush(stdout);
            ck_assert(settings[i].value_float);
        }
    }
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