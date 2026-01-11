#include "mapreduce.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

part_col_t **partition_table;
Partitioner part_func;
int num_partitions;
Reducer reduce_glob;

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
  curr_kv->key = strdup(key);
  curr_kv->value = strdup(value);
  curr_part->curr_size++;
  pthread_mutex_unlock(&curr_part->lock);
}

unsigned long MR_DefaultHashPartition(char *key, int partition_count) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % partition_count;
}

void *mapper_worker(mapper_args_t *mapper_worker) {
  // grab lock, grab file name, increment counter,
  // unlock, call map()
  work_queue_t *work_queue = mapper_worker->map_queue;
  while (1) {
    pthread_mutex_lock(&work_queue->lock);
    // we processed all files
    if (work_queue->num_files <= work_queue->next_file_idx) {
      pthread_mutex_unlock(&work_queue->lock);
      break;
    }

    char *file_name = work_queue->files[work_queue->next_file_idx];
    work_queue->next_file_idx++;

    pthread_mutex_unlock(&work_queue->lock);

    work_queue->map(file_name);
  }
  return NULL;
}

void *reduce_worker(void *i) {
  int partition_num = *(int *)i;
  free(i);
  part_col_t *part = &partition_table[partition_num];
  while (part->iter_idx < part->curr_size) {
    char *key = part->partition_arr[part->iter_idx].key;
    reduce_glob(key, get_next, partition_num);
  }
  return NULL;
}

void sort_partitions(int num_mappers) {
  for (int i = 0; i < num_mappers; i++) {
    part_col_t *map_partitions = partition_table[i];
    for (int j = 0; j < num_partitions; j++) {
      printf("");
    }
  }

  void allocate_partition_table(int num_reducers, int num_mappers) {
    part_col_t **per_map_table =
        (part_col_t **)malloc(sizeof(part_col_t *) * num_mappers);
    partition_table = per_map_table;
    assert(per_map_table != NULL);

    for (int i = 0; i < num_mappers; i++) {
      part_col_t *partition =
          (part_col_t *)malloc(sizeof(part_col_t) * num_reducers);
      assert(partition != NULL);
      per_map_table[i] = partition;

      partition->capacity = PARTITION_CAPACITY;
      partition->curr_size = 0;
      partition->iter_idx = 0;

      kv_t *part_arr = (kv_t *)malloc(sizeof(kv_t) * PARTITION_CAPACITY);
      assert(part_arr != NULL);
      partition->partition_arr = part_arr;
    }
  }

  void resize_partition(int partition, int mapper) {
    part_col_t *map_partition = partition_table[mapper];

    int old_capacity = map_partition->capacity;
    int new_capacity = old_capacity * 2;

    kv_t *new_arr = malloc(sizeof(kv_t) * new_capacity);
    assert(new_arr != NULL);

    // copy existing elements
    for (int i = 0; i < map_partition->curr_size; i++) {
      new_arr[i] = map_partition->partition_arr[i];
    }

    free(map_partition->partition_arr);
    map_partition->partition_arr = new_arr;
    map_partition->capacity = new_capacity;
  }

  void MR_Run(int argc, char *argv[], Mapper map, int num_mappers,
              Reducer reduce, int num_reducers, Partitioner partition) {
    part_func = partition;
    num_partitions = num_reducers;
    reduce_glob = reduce;

    work_queue_t mapper_queue = {.files = &argv[1],
                                 .num_files = argc - 1,
                                 .next_file_idx = 0,
                                 .map = map};

    int rc = pthread_mutex_init(&mapper_queue.lock, NULL);
    assert(rc == 0);

    pthread_t mapper_threads[num_mappers];

    // begin mapping
    for (int i = 0; i < num_mappers; i++) {
      mapper_args_t *map_args = (mapper_args_t *)malloc(sizeof(mapper_args_t));
      map_args->map_queue = &mapper_queue;
      map_args->thread_id = i;

      pthread_create(&mapper_threads[i], NULL, (void *)mapper_worker,
                     &map_args);
    }
  }
