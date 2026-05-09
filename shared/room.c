#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "room.h"

void room_manager_init(RoomManager *rm) {
    memset(rm, 0, sizeof(RoomManager));
    pthread_mutex_init(&rm->global_lock, NULL);
    for (int i = 0; i < MAX_ROOMS; i++)
        pthread_mutex_init(&rm->rooms[i].lock, NULL);
}

/* Cherche ou crée une salle. Ordre de verrou : global_lock puis room->lock. */
Room *room_get_or_create(RoomManager *rm, const char *name) {
    pthread_mutex_lock(&rm->global_lock);
    for (int i = 0; i < rm->count; i++) {
        if (strcmp(rm->rooms[i].name, name) == 0) {
            Room *r = &rm->rooms[i];
            pthread_mutex_unlock(&rm->global_lock);
            return r;
        }
    }
    if (rm->count < MAX_ROOMS) {
        Room *r = &rm->rooms[rm->count++];
        strncpy(r->name, name, MAX_ROOM - 1);
        r->name[MAX_ROOM - 1] = '\0';
        r->count = 0;
        pthread_mutex_unlock(&rm->global_lock);
        return r;
    }
    pthread_mutex_unlock(&rm->global_lock);
    return NULL;
}

/* Cherche une salle SANS la créer (évite de créer des salles fantômes). */
Room *room_find(RoomManager *rm, const char *name) {
    pthread_mutex_lock(&rm->global_lock);
    for (int i = 0; i < rm->count; i++) {
        if (strcmp(rm->rooms[i].name, name) == 0) {
            Room *r = &rm->rooms[i];
            pthread_mutex_unlock(&rm->global_lock);
            return r;
        }
    }
    pthread_mutex_unlock(&rm->global_lock);
    return NULL;
}

int room_add_client(Room *r, int fd) {
    pthread_mutex_lock(&r->lock);
    if (r->count >= MAX_CLIENTS_PER_ROOM) {
        pthread_mutex_unlock(&r->lock);
        return -1;
    }
    r->client_fds[r->count++] = fd;
    pthread_mutex_unlock(&r->lock);
    return 0;
}

void room_remove_client(Room *r, int fd) {
    pthread_mutex_lock(&r->lock);
    for (int i = 0; i < r->count; i++) {
        if (r->client_fds[i] == fd) {
            for (int j = i; j < r->count - 1; j++)
                r->client_fds[j] = r->client_fds[j + 1];
            r->count--;
            break;
        }
    }
    pthread_mutex_unlock(&r->lock);
}

/*
 * Correctif : on copie la liste des fd sous lock, puis on relâche
 * le lock AVANT d'appeler write(). Ainsi un client lent ne bloque
 * pas toute la salle.
 */
void room_broadcast(Room *r, int sender_fd, const char *msg, int msglen) {
    int fds[MAX_CLIENTS_PER_ROOM];
    int n = 0;

    pthread_mutex_lock(&r->lock);
    for (int i = 0; i < r->count; i++)
        if (r->client_fds[i] != sender_fd)
            fds[n++] = r->client_fds[i];
    pthread_mutex_unlock(&r->lock);

    for (int i = 0; i < n; i++) {
        /* Écriture complète (gestion EINTR et écritures partielles) */
        int written = 0;
        while (written < msglen) {
            int ret = (int)write(fds[i], msg + written, msglen - written);
            if (ret <= 0) break;
            written += ret;
        }
    }
}

void room_list(RoomManager *rm, char *out, int outlen) {
    pthread_mutex_lock(&rm->global_lock);
    int pos = snprintf(out, outlen, "=== Salles actives ===\n");
    for (int i = 0; i < rm->count && pos < outlen - 1; i++) {
        Room *r = &rm->rooms[i];
        pthread_mutex_lock(&r->lock);
        int cnt = r->count;
        char name[MAX_ROOM];
        strncpy(name, r->name, MAX_ROOM - 1);
        name[MAX_ROOM - 1] = '\0';
        pthread_mutex_unlock(&r->lock);
        pos += snprintf(out + pos, outlen - pos,
                        "  #%s (%d membre%s)\n", name, cnt, cnt > 1 ? "s" : "");
    }
    pthread_mutex_unlock(&rm->global_lock);
}
