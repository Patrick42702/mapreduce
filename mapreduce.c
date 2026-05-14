#include "mapreduce.h"
#include <_string.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

part_col_t **partition_table; // table for each mapper
part_col_t *master_ptable;    // combined partition table
Partitioner part_func;        // partition func
int num_partitions;           // number partitions
int num_mappers;              // number mappers
Reducer reduce_glob;          // reducer function
__thread int tls_thread_id;   // thread local storage id

char *get_next(char *key, int parition_num) {
  part_col_t *part = &master_ptable[parition_num];

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
  int tid = tls_thread_id;
  part_col_t *curr_part = &partition_table[tid][partition];
  char *key_copy = strdup(key);
  assert(key_copy != NULL);
  char *value_copy = strdup(value);
  assert(value_copy != NULL);

  // resize partition if necessary
  if (curr_part->curr_size == curr_part->capacity) {
    resize_partition(partition, tid);
  }

  kv_t *curr_kv = &(curr_part->partition_arr[curr_part->curr_size]);
  curr_kv->key = key_copy;
  curr_kv->value = value_copy;
  curr_part->curr_size++;
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

  tls_thread_id = mapper_worker->thread_id;
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
  part_col_t *part = &master_ptable[partition_num];
  while (part->iter_idx < part->curr_size) {
    char *key = part->partition_arr[part->iter_idx].key;
    reduce_glob(key, get_next, partition_num);
  }
  return NULL;
}

void allocate_master_ptable() {
  master_ptable = calloc(num_partitions, sizeof(part_col_t));
  assert(master_ptable != NULL);

  // Calculate total size needed for each partition
  int kv_p_count[num_partitions];
  memset(kv_p_count, 0, sizeof(kv_p_count));

  for (int i = 0; i < num_mappers; i++) {
    part_col_t *map_table = partition_table[i];
    for (int j = 0; j < num_partitions; j++) {
      kv_p_count[j] += map_table[j].curr_size;
    }
  }

  // Allocate arrays for each partition in master table
  for (int i = 0; i < num_partitions; i++) {
    master_ptable[i].capacity = kv_p_count[i];
    master_ptable[i].curr_size = 0;
    master_ptable[i].iter_idx = 0;
    if (kv_p_count[i] > 0) {
      master_ptable[i].partition_arr = malloc(sizeof(kv_t) * kv_p_count[i]);
      assert(master_ptable[i].partition_arr != NULL);
    } else {
      master_ptable[i].partition_arr = NULL;
    }
  }

  // Copy all key-value pairs from mapper partitions to master partitions
  for (int i = 0; i < num_mappers; i++) {
    part_col_t *map_table = partition_table[i];
    for (int j = 0; j < num_partitions; j++) {
      part_col_t *src_part = &map_table[j];
      part_col_t *dst_part = &master_ptable[j];
      
      // Copy all key-value pairs from this mapper's partition
      for (int k = 0; k < src_part->curr_size; k++) {
        dst_part->partition_arr[dst_part->curr_size] = src_part->partition_arr[k];
        dst_part->curr_size++;
      }
    }
  }
}

void sort_partitions() {
  // Sort the master partition table after merging
  for (int j = 0; j < num_partitions; j++) {
    part_col_t *curr_partition = &master_ptable[j];
    kv_t *arr = curr_partition->partition_arr;
    
    // Insertion sort
    for (int k = 1; k < curr_partition->curr_size; k++) {
      kv_t curr_kv = arr[k];
      char *key = arr[k].key;
      int x = k - 1;

      while (x >= 0 && strcmp(arr[x].key, key) > 0) {
        arr[x + 1] = arr[x];
        x = x - 1;
      }
      arr[x + 1] = curr_kv;
    }
  }
}

void allocate_partition_table() {
  part_col_t **per_map_table = malloc(sizeof(part_col_t *) * num_mappers);
  assert(per_map_table != NULL);
  partition_table = per_map_table;

  for (int i = 0; i < num_mappers; i++) {
    part_col_t *partitions = calloc(num_partitions, sizeof(*partitions));
    assert(partitions != NULL);
    partition_table[i] = partitions;
    for (int j = 0; j < num_partitions; j++) {

      partitions[j].capacity = PARTITION_CAPACITY;
      kv_t *part_arr = malloc(sizeof(kv_t) * PARTITION_CAPACITY);
      assert(part_arr != NULL);
      partitions[j].partition_arr = part_arr;
    }
  }
}

void resize_partition(int partition, int mapper) {
  part_col_t *p = &partition_table[mapper][partition];

  int old_capacity = p->capacity;
  int new_capacity = old_capacity * 2;

  kv_t *tmp = realloc(p->partition_arr, sizeof(kv_t) * new_capacity);
  assert(tmp != NULL);

  p->partition_arr = tmp;
  p->capacity = new_capacity;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers_arg, Reducer reduce,
            int num_reducers, Partitioner partition) {
  part_func = partition;
  num_partitions = num_reducers;
  num_mappers = num_mappers_arg;
  reduce_glob = reduce;

  // Allocate partition table for all mappers
  allocate_partition_table();

  work_queue_t mapper_queue = {
      .files = &argv[1], .num_files = argc - 1, .next_file_idx = 0, .map = map};

  int rc = pthread_mutex_init(&mapper_queue.lock, NULL);
  assert(rc == 0);

  pthread_t mapper_threads[num_mappers];

  // Create and start mapper threads
  for (int i = 0; i < num_mappers; i++) {
    mapper_args_t *map_args = (mapper_args_t *)malloc(sizeof(mapper_args_t));
    assert(map_args != NULL);
    map_args->map_queue = &mapper_queue;
    map_args->thread_id = i;

    rc = pthread_create(&mapper_threads[i], NULL, (void *)mapper_worker, map_args);
    assert(rc == 0);
  }

  // Wait for all mapper threads to complete
  for (int i = 0; i < num_mappers; i++) {
    pthread_join(mapper_threads[i], NULL);
  }

  // Merge mapper partitions into master partition table
  allocate_master_ptable();

  // Sort the master partitions by key
  sort_partitions();

  // Create and start reducer threads
  pthread_t reducer_threads[num_reducers];
  for (int i = 0; i < num_reducers; i++) {
    int *partition_num = malloc(sizeof(int));
    assert(partition_num != NULL);
    *partition_num = i;
    
    rc = pthread_create(&reducer_threads[i], NULL, reduce_worker, partition_num);
    assert(rc == 0);
  }

  // Wait for all reducer threads to complete
  for (int i = 0; i < num_reducers; i++) {
    pthread_join(reducer_threads[i], NULL);
  }

  // Cleanup: free all allocated memory
  pthread_mutex_destroy(&mapper_queue.lock);

  // Free mapper partition tables
  for (int i = 0; i < num_mappers; i++) {
    for (int j = 0; j < num_partitions; j++) {
      part_col_t *part = &partition_table[i][j];
      for (int k = 0; k < part->curr_size; k++) {
        free(part->partition_arr[k].key);
        free(part->partition_arr[k].value);
      }
      free(part->partition_arr);
    }
    free(partition_table[i]);
  }
  free(partition_table);

  // Free master partition table
  for (int i = 0; i < num_partitions; i++) {
    free(master_ptable[i].partition_arr);
  }
  free(master_ptable);
}
