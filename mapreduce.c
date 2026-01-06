#include "mapreduce.h"
#include <assert.h>
#include <pthread.h>

void MR_Emit(char *key, char *value) {}

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

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce,
            int num_reducers, Partitioner partition) {

  work_queue_t mapper_queue = {
      .files = &argv[1], .num_files = argc, .next_file_idx = 1, .map = map};

  int rc = pthread_mutex_init(&mapper_queue.lock, NULL);
  assert(rc == 0);

  pthread_t mapper_threads[num_mappers];

  for (int i = 0; i < num_mappers; i++) {
    pthread_create(&mapper_threads[i], NULL, (void *)mapper_worker,
                   &mapper_queue);
  }

  for (int i = 0; i < num_mappers; i++) {
    pthread_join(mapper_threads[i], NULL);
  }
}
