#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../includes/parsing_conf.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



int main (int argc, char **argv){
    // DANGER: UPDATE ON EVERY NEW SETTING
    //TODO: ON EVERY NEW OPTION MAKE SURE EVERYTHING WORKS PROPERLY
    char str[] = {"../../config.txt"};
    char path[] ={ "/temp/" };
    Server_conf  all_settings[num_avail_settings];
    init_serv_conf(all_settings);
    init_settings_arr(all_settings);
    parse_file(str, all_settings);
    int res_sc = 0;
    SYSCALL_EXIT("mkdir", res_sc, mkdir(path, 0744));

    return EXIT_SUCCESS;
}