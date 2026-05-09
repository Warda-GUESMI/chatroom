#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

#define QUEUE_SIZE 128

typedef struct {
    int             items[QUEUE_SIZE];
    int             head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} Queue;

void queue_init(Queue *q);
void queue_push(Queue *q, int fd);
int  queue_pop(Queue *q);
void queue_destroy(Queue *q);

#endif
