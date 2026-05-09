#ifndef ROOM_H
#define ROOM_H

#include <pthread.h>
#include "message.h"

#define MAX_ROOMS            16
#define MAX_CLIENTS_PER_ROOM 32

/*
 * Règle d'ordre de verrouillage (pour éviter les deadlocks) :
 *   rm->global_lock  AVANT  room->lock
 * Ne jamais acquérir global_lock en tenant déjà room->lock.
 */
typedef struct {
    char            name[MAX_ROOM];
    int             client_fds[MAX_CLIENTS_PER_ROOM];
    int             count;
    pthread_mutex_t lock;
} Room;

typedef struct {
    Room            rooms[MAX_ROOMS];
    int             count;
    pthread_mutex_t global_lock;
} RoomManager;

void  room_manager_init(RoomManager *rm);
Room *room_get_or_create(RoomManager *rm, const char *name);
Room *room_find(RoomManager *rm, const char *name);   /* ne crée pas */
int   room_add_client(Room *r, int fd);
void  room_remove_client(Room *r, int fd);
void  room_broadcast(Room *r, int sender_fd, const char *msg, int msglen);
void  room_list(RoomManager *rm, char *out, int outlen);

#endif
