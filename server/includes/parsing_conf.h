#ifndef parsing_conf_h
#define parsing_conf_h
#include <string.h>

// DANGER: UPDATE EVERY NEW SETTING
#define num_avail_settings 4
// DANGER: UPDATE EVERY NEW SETTING
//TODO: ON EVERY NEW OPTION MAKE SURE EVERYTHING WORKS PROPERLY
#define LONGEST_STR 14

typedef struct {
    unsigned long int value;
    //0 string as argument 1 number as argument
    unsigned short int str_or_int;
    unsigned char *value_string;
    unsigned char setting[LONGEST_STR];

} Server_conf;



#define init_serv_conf(st)\
    memset(&st, 0, sizeof(Server_conf));

void init_settings_arr(Server_conf *settings);
Server_conf *parse_file(const char *file, Server_conf *where_to_save);

#endif // !parsing_cong_h