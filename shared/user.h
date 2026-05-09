#ifndef USER_H
#define USER_H

#include <pthread.h>
#include "message.h"

#define MAX_USERS 64

/*
 * Chaque User a son propre mutex pour protéger ses champs (nick, current_room).
 * Le mutex global de UserManager protège uniquement la liste (active, count).
 * Règle d'ordre de verrouillage : um->lock AVANT user->lock (jamais l'inverse).
 */
typedef struct {
    int             fd;
    char            nick[MAX_NAME];
    char            current_room[MAX_ROOM];
    int             active;
    pthread_mutex_t lock;   /* protège nick et current_room */
} User;

typedef struct {
    User            users[MAX_USERS];
    int             count;
    pthread_mutex_t lock;   /* protège active / count */
} UserManager;

void  user_manager_init(UserManager *um);
User *user_add(UserManager *um, int fd, const char *nick);
User *user_find_by_fd(UserManager *um, int fd);
User *user_find_by_nick(UserManager *um, const char *nick);
void  user_remove(UserManager *um, int fd);
void  user_list(UserManager *um, char *out, int outlen);

#endif
