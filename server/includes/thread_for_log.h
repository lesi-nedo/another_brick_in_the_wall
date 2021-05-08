#ifndef thread_for_log_h
#define thread_for_log_h


#define LOGS "../../logs/"
#define READ 0
#define WRITE 1
#define MAX_ITER_THEN_STOP 20

typedef struct {
    //if set to 1 then a signal to creator of the thread has arrived ⦟
    //it needs to exit.
    int sign;
    char *file_name;
    int pipe[2];
} Arg_log_th;

void *log_to_file(void *arg_th);

#endif // !thread_for_log_h