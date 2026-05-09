#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "queue.h"
#include "../shared/room.h"
#include "../shared/user.h"

#define POOL_SIZE 8

typedef struct {
    pthread_t    threads[POOL_SIZE];
    Queue        task_queue;
    RoomManager *room_mgr;
    UserManager *user_mgr;
    int          running;
} ThreadPool;

void thread_pool_init(ThreadPool *tp, RoomManager *rm, UserManager *um);
void thread_pool_submit(ThreadPool *tp, int client_fd);
void thread_pool_destroy(ThreadPool *tp);

#endif
