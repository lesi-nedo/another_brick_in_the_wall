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
 * @param: argument from setting name that represents the configure i.e a value as a string.
 * @return: 1 if string is not valid 0 otherwise.
 */
static short is_valid_socket (char *arg, void *value){
    if(arg==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    char **cast_val =  (char **) value;
    while(isspace(*arg) || *arg == '"') arg++;
    size_t len_str = strlen(arg)+1;
    size_t size = 0;
    while(size < len_str && (isalpha(*(arg+size)) || *(arg+size) == '-' || *(arg+size) == '_' || *(arg+size) == '.')) size++;
    if(size == 0) return 1;
    size_t after = size;
    while(after < len_str && (isspace(*(arg+after)) || *(arg+size) == '"')) after++;
    if(after < len_str && *(arg+after) != '\n' && *(arg+after) != '\0') return 1;
    arg[size] = '\0';
    *cast_val = (char *)calloc(size+1, sizeof(char));
    CHECK_EQ_EXIT("Calling malloc in is_valid_string", *cast_val, NULL, "run out of memory.\n", NULL);
    strncpy((char *)*cast_val, arg, size);
    return 0;
}

static short is_valid_log_file (char *arg, void *value){
    if(arg==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    char **cast_val =  (char **) value;
    while(isspace(*arg) || *arg == '"') arg++;
    size_t len_str = strlen(arg)+1;
    size_t size = 0;
    //0 the there is no . in the name 1 there is and with add log in end
    //2 there is just a dot
    //Has to be 1 if not valgrind will complain
    int ext = 1;
    //if 0 then it will ad log extension
    while(size < len_str && (isalpha(*(arg+size)) || *(arg+size) == '-' || *(arg+size) == '_')) size++;
    if(size == 0) return 1;
    if(*(arg+size) == '.'){
        size++;
        if((size+3) < len_str){
            arg[size++] = 'l';
            arg[size++] = 'o';
            arg[size++] = 'g';
        } else ext = 4;
    } else ext = 5;
    size_t after = size;
    while(after < len_str && (isspace(*(arg+after)) || *(arg+size) == '"')) after++;
    if(after < len_str && *(arg+after) != '\n' && *(arg+after) != '\0') return 1;
    arg[size] = '\0';
    *cast_val = (char *)calloc(size+ext,sizeof(char));
    CHECK_EQ_EXIT("Calling malloc in is_valid_string", *cast_val, NULL, "run out of memory.\n", NULL);
    strncpy((char *)*cast_val, arg, size);
    if(ext == 4){
        (*cast_val)[size] = 'l';
        (*cast_val)[size+1] = 'o';
        (*cast_val)[size+2] = 'g';
        (*cast_val)[size+3] = '\0';
    } else if(ext == 5){
        (*cast_val)[size] = '.';
        (*cast_val)[size+1] = 'l';
        (*cast_val)[size+2] = 'o';
        (*cast_val)[size+3] = 'g';
        (*cast_val)[size+4] = '\0';
    }
    return 0;
}

/** 
 * @param: string to be validated
 * @return: 1 if string is not valid 0 otherwise.
*/
static short int is_valid_max_size(char *arg, void *value){
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
    if(space < len_str && (isalnum(*(arg+space)) || ispunct(*(arg+space))) && *(arg+space) != '#') return 1;
    arg[size] = '\0';
    if(isNumber(arg, &arg_to_ass) != 0){
        return 1;
    }
    *(long *)value = arg_to_ass * moltp;
    return 0;
}

static short int is_valid_max_file(char *arg, void *value){
    if(arg==NULL){
        printf("\033[1;31m null argument in is_valid_set.\033[0;37m\n");
        return 0;
    }
    while(isspace(*arg)) arg++;
    size_t len_str = strlen(arg)+1;
    size_t size =0;
    size_t space = 0;
    long arg_to_ass = 0;
    while(space < len_str && isdigit(*(arg+size))) size++;
    if(size == 0) return 1;//return because there are no digits
    space += size;
    while(space < len_str && isspace(*(arg+space))) space++;
    if(space < len_str && (isalnum(*(arg+space)) || ispunct(*(arg+space))) && *(arg+space) != '#') return 1;
    arg[size] = '\0';
    if(isNumber(arg, &arg_to_ass) != 0){
        return 1;
    }
    *(long *)value = arg_to_ass;
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
void parse_file(const char *file, Server_conf *where_to_save){
    if(where_to_save == NULL || file == NULL){
        printf("\033[1;31mCalled parse_file with one NULL argument.\n\033[0;37m");
        return;
    }
    FILE *fl = fopen(file,"r");
    ssize_t ret_get_line = 0;
    size_t len_line = 0;
    char *conf_line = NULL;
    //If this variable is 1 that something in config.txt was wrong so we exit.
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
            printf("Error: not recognized setting %s in config.txt\033[1;36m(but I'm still going :) )\033[0;37m\n", set);
            continue;
        }
        if(is_valid_set((char *)set) == 1){
            is_error = 1;
            printf("\033[1;31m");
            printf("Error: invalid setting %s in config.txt\n", set);
            printf("\033[0;32m");
            printf("Usage: strings in upper with _ as delimiter.\n");
            printf("\033[0;37m");
            continue;
        }
        if(where_to_save[hashed_val].str_or_int == 1){
            if(where_to_save[hashed_val].validator(arg, (void *) &valid_arg_int)){
                is_error = 1;
                printf("\033[1;31m");
                printf("Error:  invalid argument of setting name \033[1;35m %s \033[1;31min config.txt\n", set);
                printf("\033[0;32m");
                printf("Usage: positive integers");
                if(hashed_val == MAX_ACCUM_INDEX) printf(" plus format [kb[KB], mb[MB], gb[GB]]\033[0;37m\n");
                else printf(".\n");
                printf("\033[0;37m");
                continue;
            }
            where_to_save[hashed_val].value =valid_arg_int;
        } else {
            if(where_to_save[hashed_val].validator(arg, (void *) &where_to_save[hashed_val].value_string) == 1){
                is_error = 1;
                printf("\033[1;31m");
                printf("Error:  invalid argument of setting name \033[1;35m %s \033[1;31min config.txt\n", set);
                printf("\033[0;32m");
                printf("Usage: string with only letters and a delimiter that can be '-' or '_'\033[0;37m\n");
                continue;
            }
        }
        
        // printf("\033[1m%zd\033[0m <---- ", hashed_val);
        // printf("%s\n", set);
    }
    fclose(fl);
    free(conf_line);
    if(errno != 0){
        
        print_error("getline failed %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(is_error) {
            for(size_t i = 0; i< num_avail_settings; i++){
                if(!where_to_save[i].str_or_int){
                    if(where_to_save[i].value_string != NULL){
                        free(where_to_save[i].value_string);
                    }
                }
            }
            printf("\033[1;37mFix errors in config.txt then rerun me.\n\033[0;37m");
            exit(EXIT_FAILURE);
    }
    return;
}


/** 
 * @param: array where will be stored the settings
 * @return: void
*/
void init_settings_arr(Server_conf *settings){
    //TODO: UPDATE ON EVERY NEW SETTING
    unsigned char available_settings[num_avail_settings][LONGEST_STR] = {"MAX_ACC_SIZE",\
    "MAX_NUM_FILES", "WORKERS", "SOCKET_NAME", "LOG_FILE_NAME"};
    //TODO:CAST AS VOID * & THE IN FUNCTION HANDLER CAST AS CHAR ** IF OPTION HAS A STRING ARGUMENT.
    for(size_t i=0; i<num_avail_settings; i++){
        unsigned long hashed_val = hash(available_settings[i])%BASE_MOD%num_avail_settings;
        strncpy((char *)settings[hashed_val].setting, (char *)available_settings[i], LONGEST_STR);
        //EVERY TIME MAKE SURE THAT SETTINGS ARE HASHED CORRECTLY
        // printf("HASHED: %zd STRING: %s\n", hashed_val, available_settings[i]);
        switch(i){
            case 0:
                settings[hashed_val].value_string = NULL;
                settings[hashed_val].value = 0;
                settings[hashed_val].str_or_int = 1;
                settings[hashed_val].validator = is_valid_max_size;
                break;
            case 1:
                settings[hashed_val].value_string = NULL;
                settings[hashed_val].value = 0;
                settings[hashed_val].str_or_int = 1;
                settings[hashed_val].validator = is_valid_max_file;
                break;
            case 2:
                settings[hashed_val].value_string = NULL;
                settings[hashed_val].value = 0;
                settings[hashed_val].str_or_int = 1;
                settings[hashed_val].validator = is_valid_max_file;
                break;
            case 3:
                settings[hashed_val].value_string = NULL;
                settings[hashed_val].value = 0;
                settings[hashed_val].str_or_int = 0;
                settings[hashed_val].validator = is_valid_socket;
                break;
            case 4:
                settings[hashed_val].value_string = NULL;
                settings[hashed_val].value = 0;
                settings[hashed_val].str_or_int = 0;
                settings[hashed_val].validator = is_valid_log_file;
                break;
            default:
                printf("\033[1;35mSomething went terribly wrong.\n");
                break;
        }
    }
}