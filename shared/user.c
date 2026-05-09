#include <stdio.h>
#include <string.h>
#include "user.h"

void user_manager_init(UserManager *um) {
    memset(um, 0, sizeof(UserManager));
    pthread_mutex_init(&um->lock, NULL);
    for (int i = 0; i < MAX_USERS; i++)
        pthread_mutex_init(&um->users[i].lock, NULL);
}

/*
 * Ajoute un utilisateur. Retourne un pointeur vers le slot alloué.
 * Le pointeur reste valide tant que active==1 ; ne jamais le stocker
 * au-delà d'une section critique sans relire active sous lock.
 */
User *user_add(UserManager *um, int fd, const char *nick) {
    pthread_mutex_lock(&um->lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (!um->users[i].active) {
            User *u = &um->users[i];
            pthread_mutex_lock(&u->lock);
            u->fd     = fd;
            u->active = 1;
            strncpy(u->nick, nick, MAX_NAME - 1);
            u->nick[MAX_NAME - 1] = '\0';
            strncpy(u->current_room, "general", MAX_ROOM - 1);
            u->current_room[MAX_ROOM - 1] = '\0';
            um->count++;
            pthread_mutex_unlock(&u->lock);
            pthread_mutex_unlock(&um->lock);
            return u;
        }
    }
    pthread_mutex_unlock(&um->lock);
    return NULL;
}

/*
 * Recherche par fd. Retourne le pointeur avec um->lock relâché.
 * L'appelant doit prendre u->lock avant d'accéder aux champs.
 */
User *user_find_by_fd(UserManager *um, int fd) {
    pthread_mutex_lock(&um->lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (um->users[i].active && um->users[i].fd == fd) {
            User *u = &um->users[i];
            pthread_mutex_unlock(&um->lock);
            return u;
        }
    }
    pthread_mutex_unlock(&um->lock);
    return NULL;
}

/*
 * Recherche par nick (copie sous lock pour éviter TOCTOU).
 */
User *user_find_by_nick(UserManager *um, const char *nick) {
    pthread_mutex_lock(&um->lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (um->users[i].active) {
            pthread_mutex_lock(&um->users[i].lock);
            int match = (strcmp(um->users[i].nick, nick) == 0);
            pthread_mutex_unlock(&um->users[i].lock);
            if (match) {
                User *u = &um->users[i];
                pthread_mutex_unlock(&um->lock);
                return u;
            }
        }
    }
    pthread_mutex_unlock(&um->lock);
    return NULL;
}

void user_remove(UserManager *um, int fd) {
    pthread_mutex_lock(&um->lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (um->users[i].active && um->users[i].fd == fd) {
            pthread_mutex_lock(&um->users[i].lock);
            memset(&um->users[i].nick, 0, sizeof(um->users[i].nick));
            memset(&um->users[i].current_room, 0, sizeof(um->users[i].current_room));
            um->users[i].fd     = -1;
            um->users[i].active = 0;
            pthread_mutex_unlock(&um->users[i].lock);
            um->count--;
            break;
        }
    }
    pthread_mutex_unlock(&um->lock);
}

void user_list(UserManager *um, char *out, int outlen) {
    pthread_mutex_lock(&um->lock);
    int pos = snprintf(out, outlen, "=== Utilisateurs connectes ===\n");
    for (int i = 0; i < MAX_USERS && pos < outlen - 1; i++) {
        if (um->users[i].active) {
            char nick[MAX_NAME], room[MAX_ROOM];
            pthread_mutex_lock(&um->users[i].lock);
            strncpy(nick, um->users[i].nick, MAX_NAME - 1);
            nick[MAX_NAME - 1] = '\0';
            strncpy(room, um->users[i].current_room, MAX_ROOM - 1);
            room[MAX_ROOM - 1] = '\0';
            pthread_mutex_unlock(&um->users[i].lock);
            pos += snprintf(out + pos, outlen - pos,
                            "  @%s (salle: #%s)\n", nick, room);
        }
    }
    pthread_mutex_unlock(&um->lock);
}
