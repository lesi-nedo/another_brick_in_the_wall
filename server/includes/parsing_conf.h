#ifndef parsing_conf_h
#define parsing_conf_h
#include <string.h>

// DANGER: UPDATE EVERY NEW SETTING
#define num_avail_settings 5
// DANGER: UPDATE EVERY NEW SETTING
//TODO: ON EVERY NEW OPTION MAKE SURE EVERYTHING WORKS PROPERLY
#define LONGEST_STR 14
//DANGER: ERROR PRONE 
#define BASE_MOD 52 //NEEDS TO BE CHANGE EVERY TIME THAT ADDS A FUNCTIONALITY TO CONF FILE
//TODO: REMEMBER IF YOU ADD NEW OPTION TO UPDATE FUNCTION: parse_file, init_settings_arr, MAX_ACCUM_INDE
#define MAX_ACCUM_INDEX 1 //TODO: CHANGE ON EVERY NEW OPTION


typedef struct {
    unsigned long int value;
    //0 string as argument 1 number  as argument
    unsigned short int str_or_int;
    char *value_string;
    short int (*validator)(char *str, void *value);
    unsigned char setting[LONGEST_STR];

} Server_conf;



#define init_serv_conf(st)\
    memset(&st, 0, sizeof(Server_conf));

void init_settings_arr(Server_conf *settings);
void parse_file(const char *file, Server_conf *where_to_save);
unsigned long hash(unsigned char *str);

#endif // !parsing_cong_h