#ifndef __RTM_X86_H__
#define __RTM_X86_H__
#include <pthread.h>
#include "rigel.h"
#include "rigel-tasks.h"

extern pthread_mutex_t rtmx86_barrier_count_mutex;
extern int rtmx86_barrier_count;
extern int rtmx86_barrier_global_sense;
extern int rtmx86_barrier_local_sense[CORES_TOTAL];

// This is included to bootstrap multiple threads on x86 machines
void X86_TASK_MODEL_INIT();
void X86_TASK_MODEL_WAIT();

// This is the function pointer that needs to be declared by the user
extern int rigel_main();
void *rtmx86_emulated_core_thread();

// Holds a reference to each emulated core
pthread_t *rtmx86_thread_id;
// Key for the core number associated with each thread
pthread_key_t rtmx86_core_num_key;

typedef struct rtmx86_thread_funct_data_t {
  int core_num;
} rtmx86_thread_funct_data_t;

#endif
