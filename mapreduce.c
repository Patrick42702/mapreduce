#include "mapreduce.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

part_col_t *partition_table;
Partitioner part_func;
int num_partitions;

char *get_next(char *key, int parition_num) {
  part_col_t *part = &partition_table[parition_num];

  // iterator is completed
  if (part->iter_idx >= part->curr_size) {
    return NULL;
  }

  kv_t *kv = &part->partition_arr[part->iter_idx];

  // if key is the same, end reduction call
  if (strcmp(kv->key, key) != 0)
    return NULL;

  part->iter_idx++;
  return kv->value;
}

void MR_Emit(char *key, char *value) {
  int partition = part_func(key, num_partitions);
  part_col_t *curr_part = &partition_table[partition];

  pthread_mutex_lock(&curr_part->lock);

  // resize partition if necessary
  if (curr_part->curr_size == curr_part->capacity) {
    resize_partition(partition);
  }

  kv_t *curr_kv = &(curr_part->partition_arr[curr_part->curr_size]);
  curr_kv->key = key;
  curr_kv->value = value;
  curr_part->curr_size++;
  pthread_mutex_unlock(&curr_part->lock);
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % num_partitions;
}

void *mapper_worker(work_queue_t *work_queue) {
  // grab lock, grab file name, increment counter,
  // unlock, call map()
  while (1) {

    // we processed all files
    if (work_queue->num_files < work_queue->next_file_idx) {
      pthread_mutex_unlock(&work_queue->lock);
      break;
    }

    pthread_mutex_lock(&work_queue->lock);

    char *file_name = work_queue->files[work_queue->next_file_idx];
    work_queue->next_file_idx++;

    pthread_mutex_unlock(&work_queue->lock);

    work_queue->map(file_name);
  }
  return NULL;
}

void sort_partition() {
  for (int i = 0; i < num_partitions; i++) {
    part_col_t *part = &partition_table[i];
  }
}

void allocate_partition_table(int num_reducers) {
  partition_table = (part_col_t *)malloc(sizeof(part_col_t) * num_reducers);
  for (int i = 0; i < num_reducers; i++) {
    part_col_t *curr_partition = &partition_table[i];
    assert(curr_partition != NULL);

    curr_partition->capacity = 16;
    curr_partition->curr_size = 0;
    curr_partition->iter_idx = 0;

    int rc = pthread_mutex_init(&curr_partition->lock, NULL);
    assert(rc == 0);

    kv_t *kv_arr = (kv_t *)malloc(sizeof(kv_t) * PARTITION_CAPACITY);
    assert(kv_arr != NULL);
    curr_partition->partition_arr = kv_arr;
  }
}

void resize_partition(int partition) {
  part_col_t *part = &partition_table[partition];

  int old_capacity = part->capacity;
  int new_capacity = old_capacity * 2;

  kv_t *new_arr = malloc(sizeof(kv_t) * new_capacity);
  assert(new_arr != NULL);

  // copy existing elements
  for (int i = 0; i < part->curr_size; i++) {
    new_arr[i] = part->partition_arr[i];
  }

  free(part->partition_arr);
  part->partition_arr = new_arr;
  part->capacity = new_capacity;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce,
            int num_reducers, Partitioner partition) {

  part_func = partition;
  num_partitions = num_reducers;

  work_queue_t mapper_queue = {
      .files = &argv[1], .num_files = argc, .next_file_idx = 1, .map = map};

  int rc = pthread_mutex_init(&mapper_queue.lock, NULL);
  assert(rc == 0);

  pthread_t mapper_threads[num_mappers];

  allocate_partition_table(num_reducers);

  for (int i = 0; i < num_mappers; i++) {
    pthread_create(&mapper_threads[i], NULL, (void *)mapper_worker,
                   &mapper_queue);
  }

  for (int i = 0; i < num_mappers; i++) {
    pthread_join(mapper_threads[i], NULL);
  }
}
