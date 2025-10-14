#ifndef THREAD_ARGS_H
#define THREAD_ARGS_H

#include <stdbool.h>

typedef struct {
    bool is_background; 
    char **argv;       

} thread_args_t;

#endif // THREAD_ARGS_H
