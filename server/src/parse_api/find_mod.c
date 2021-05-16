#include <stdio.h>
#include <stdlib.h>
#include "../../includes/parsing_conf.h"
#include "../../includes/util.h"
#include <math.h>


int yes(int arr[], size_t size){
    int zero = 0;
    for(size_t i=0; i < size; i++){
        if(arr[abs(arr[i])] > 0){
            arr[abs(arr[i])] = -arr[abs(arr[i])];
        } else if(zero < 2 && arr[abs(arr[i])] == 0){
            zero++;
        } else return 0;
    }
    return 1;
}


int main(int argc, char **argv){
    if(argc !=3){
        fprintf(stderr, "Insert a range.\n");
        exit(EXIT_FAILURE);
    }
    long r = 0;
    long s = 0;
    isNumber(argv[1], &r);
    isNumber(argv[2], &s);
    int arr[num_avail_settings];
    for(size_t j=r; j<s; j++){
        for(size_t i = 0; i < num_avail_settings; i++){
            arr[i] = hash(available_settings[i])%j%num_avail_settings;
        }
        if(yes(arr, num_avail_settings)){
            printf("FOUND: %zd ", j);
            printf("[");
            for(size_t i = 0; i < num_avail_settings; i++) printf(" %d", arr[i]);
            printf("]\n");
        }   
    }
}