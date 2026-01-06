#ifndef __mapreduce_h__
#define __mapreduce_h__
#include <pthread.h>

// Different function pointer types used by MR
typedef char *(*Getter)(char *key, int partition_number);
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, Getter get_func, int partition_number);
typedef unsigned long (*Partitioner)(char *key, int num_partitions);

typedef struct {
  char **files;
  int num_files;
  int next_file_idx;
  Mapper map;
  pthread_mutex_t lock;
} work_queue_t;

void *mapper_worker(work_queue_t *work_queue);

typedef struct {
  char *key;
  char *value;
} kv_t;

#define PARTITION_CAPACITY 16

typedef struct {
  int curr_size;
  int capacity;
  int iter_idx;
  pthread_mutex_t lock;
  kv_t *partition_arr;
} part_col_t;

void allocate_partition_table(int num_reducers);

void resize_partition(int partition);

// External functions: these are what you must define
void MR_Emit(char *key, char *value);

unsigned long MR_DefaultHashPartition(char *key, int num_partitions);

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce,
            int num_reducers, Partitioner partition);

#endif // __mapreduce_h__
