#include "../../includes/parsing_conf.h"
#include <stdio.h>
#include "../../includes/util.h"
#include <errno.h>
#include <ctype.h>


/** 
 * @param: string to be validated
 * @return: 1 if string is not valid 0 otherwise.
*/

static short int is_valid_set (char *str){
    if(str==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    size_t size = strlen(str);
    size_t i = 0;
    while(i < size){ 
        if(!isupper(str[i]) && str[i] != '_') return 1;
        i++;
    }
    return 0;

}

/**
 * @param: argument from setting name that rappresentsthe configure i.e a value as a string.
 * @return: 1 if string is not valid 0 otherwise.
 */
static short is_valid_string (char *arg, char **value){
    if(arg==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    while(isspace(*arg) || *arg == '"') arg++;
    size_t len_str = strlen(arg)+1;
    size_t size = 0;
    while(size < len_str && (isalpha(*(arg+size)) || *(arg+size) == '-' || *(arg+size) == '_' || *(arg+size) == '.')) size++;
    if(size == 0) return 1;
    size_t after = size;
    while(after < len_str && (isspace(*(arg+after)) || *(arg+size) == '"')) after++;
    if(after < len_str && *(arg+after) != '\n' && *(arg+after) != '\0') return 1;
    arg[size] = '\0';
    *value = (char *)malloc(sizeof(*value)*size);
    CHECK_EQ_EXIT("Calling malloc in is_valid_string", value, NULL, "run out of memory.\n", NULL);
    strncpy(*value, arg, size);
    return 0;
}

/** 
 * @param: string to be validated
 * @return: 1 if string is not valid 0 otherwise.
*/
static short int is_valid_int(char *arg, void *res, int exten){
    if(arg==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    while(isspace(*arg)) arg++;
    size_t len_str = strlen(arg)+1;
    size_t size =0;
    int moltp = 1;
    size_t space = 0;
    long arg_to_ass = 0;
    while(isdigit(*(arg+size)) && space < len_str) size++;
    if(size == 0) return 1;//return because there are no digits
    space += size;
    while(space < len_str && isspace(*(arg+space))) space++;
    //TODO: MAX_ACC_SIZE HAS TO BE AT INDEX O IN THE ARRAY. THIS MAY BE CAUSE OF FUTERE BUGS
    if(exten == 0){
        if(space +2 >= len_str) return 1;
        char to_check = tolower(*(arg+space));
        char to_check_next = tolower(*(arg+space+1));
        if(to_check_next != 'b') return 1;
        switch(to_check){
            case 'm':
                moltp = 1000;
            break;
            case 'g':
                moltp=1e+6;
            break;
            default:
                return 1;
        }
        space += 2;
    }
    if(space < len_str && (isalnum(*(arg+space)) || ispunct(*(arg+space))) && *(arg+space) != '#') return 1;
    arg[size] = '\0';
    if(isNumber(arg, &arg_to_ass) != 0){
        return 1;
    }
    *(long *)res = arg_to_ass * moltp;
    return 0;
}

/** 
 * @author:Dan Bernstein
 * @website: http://www.cse.yorku.ca/~oz/hash.html
 * @param: take a string to hash it
*/
    unsigned long
    hash(unsigned char *str)
    {
        unsigned long hash = 5381;
        int c;

        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash;
    }

/**
 * @param: file pathname to the file absolute or relative.
 * @param: where_to_save: struct that holds values read from conf file.
 * @return: struct of type Server_conf with all values found in config.txt file that are valid
 */
Server_conf *parse_file(const char *file, Server_conf *where_to_save){
    if(where_to_save == NULL || file == NULL){
        printf("\033[1;31mCalled parse_file with one NULL argument.\n\033[0;37m");
        return NULL;
    }


    FILE *fl = fopen(file,"r");
    ssize_t ret_get_line = 0;
    size_t len_line = 0;
    char *conf_line = NULL;
    //If this variable is 1 that something in conf.txt was wrong so we exit.
    int is_error = 0;

    CHECK_EQ_EXIT("calling fopen", fl, NULL, "Was not possible to open %s\n", file);
    while((errno = 0, ret_get_line = getline(&conf_line, &len_line, fl)) != -1){
        if(*conf_line == '#' ) continue; //skipping comments
        size_t char_span = strcspn(conf_line, "=");// number of elements before =
        //Here is stored the name of the setting.
        unsigned char *set =  (unsigned char *)conf_line;
        set[char_span] = '\0';
        //Here is stored the argument
        char *arg = conf_line+char_span+1;
        long valid_arg_int = 0;
        //Hashed setting
        unsigned long hashed_val = hash(set)%BASE_MOD%num_avail_settings;
        //If the name of the setting is not in array then returns 
        if(strncmp((char *)where_to_save[hashed_val].setting, (char *) set, LONGEST_STR)) {
            printf("\033[1;31m");
            printf("Error: not recognized setting %s in config.txt\n", set);
            printf("\033[0;32m");
            is_error =0;
            continue;
        }
        if((is_error = is_valid_set((char *)set)) == 1){
            printf("\033[1;31m");
            printf("Error: invalid setting %s in config.txt\n", set);
            printf("\033[0;32m");
            printf("Usage: strings in upper with _ as delimeter.\n");
            printf("\033[0;37m");
            continue;
        }
        //TODO: MAX_ACC_SIZE HAS TO BE AT INDEX O IN THE ARRAY. THIS MAY BE CAUSE OF FUTERE BUGS
        if(where_to_save[hashed_val].str_or_int == 1){
            if((is_error =is_valid_int(arg, &valid_arg_int, hashed_val))){
                printf("\033[1;31m");
                printf("Error:  invalid argument of setting name \033[1;35m %s \033[1;31min config.txt\n", set);
                printf("\033[0;32m");
                printf("Usage: positive integers");
                if(hashed_val == 0) printf(" plus format [kb[KB], mb[MB], gb[GB]]\n");
                else printf(".\n");
                printf("\033[0;37m");
                continue;
            }
            where_to_save[hashed_val].value =valid_arg_int;
        } else {
            if((is_error = is_valid_string(arg, &where_to_save[hashed_val].value_string)) == 1){
                printf("\033[1;31m");
                printf("Error:  invalid argument of setting name \033[1;35m %s \033[1;31min config.txt\n", set);
                printf("\033[0;32m");
                printf("Usage: valid string is only letters with a delemiter that can be '-' or '_'\033[0;37m\n");
                continue;
            }
        }
        // printf("\033[1m%zd\033[0m <---- ", hashed_val);
        // printf("%s\n", set);
    }
    if(errno != 0){
        print_error("getline failed %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fclose(fl);
    free(conf_line);
     if(is_error) {
            printf("\033[1;37mFix errors in conf.txt then rerun me.\n\033[0;37m");
            exit(EXIT_FAILURE);
        }
    return NULL;
}


/** 
 * @param: array where will be stored the settings
 * @return: void
*/
void init_settings_arr(Server_conf *settings){
    //TODO: UPDATE ON EVERY NEW SETTING
    unsigned char available_settings[num_avail_settings][LONGEST_STR] = {"MAX_ACC_SIZE",\
    "MAX_NUM_FILES", "WORKERS", "SOCKET_NAME"};
    for(size_t i=0; i<num_avail_settings; i++){
        unsigned long hashed_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        strncpy((char *)settings[hashed_val].setting, (char *)available_settings[i], LONGEST_STR);
        //EVERY TIME MAKE SURE THAT SETTINGS ARE HASHED CORRECTlY
        // printf("HASHED: %zd STRING: %s\n", hashed_val, available_settings[i]);
        if(i < 3) settings[hashed_val].str_or_int = 1;
        else settings[hashed_val].str_or_int = 0;
    }
}

// int main (void){
//     char str[] = {"../../config.txt"};
//     Server_conf  all_settings[num_avail_settings];
//     init_serv_conf(all_settings);
//     init_settings_arr(all_settings);
//     parse_file(str, all_settings);
//     // int i =0;
//     // while(i < num_avail_settings){
//     //     if(all_settings[i].str_or_int){
//     //         printf("%zd\n", all_settings[i].value);
//     //     } else printf("%s\n", all_settings[i].value_string);
//     //     i++;
//     // }
// }
