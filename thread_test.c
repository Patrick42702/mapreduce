#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
  int a;
  int b;
} myarg_t;
typedef struct {
  int x;
  int y;
} myret_t;

void *mythread(void *arg) {
  pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&lock);
  myret_t *ret_vals = malloc(sizeof(myret_t));
  printf("%s %d \n", "This was called from", getpid());
  ret_vals->x = 1;
  ret_vals->y = 2;
  return (void *)ret_vals;
}

int main(int argc, char *argv[]) {
  pthread_t my_p1;
  pthread_t my_p2;
  pthread_create(&my_p1, NULL, mythread, NULL);
  pthread_create(&my_p2, NULL, mythread, NULL);
  pthread_join(my_p1, NULL);
  pthread_join(my_p1, NULL);
  return 0;
}
