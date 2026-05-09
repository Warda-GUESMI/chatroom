#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include "thread_pool.h"
#include "../shared/protocol.h"

/* Compteur atomique pour générer des nicks uniques par défaut. */
static uint64_t s_uid_counter = 0;
static pthread_mutex_t s_uid_lock = PTHREAD_MUTEX_INITIALIZER;

static uint64_t next_uid(void) {
    pthread_mutex_lock(&s_uid_lock);
    uint64_t id = ++s_uid_counter;
    pthread_mutex_unlock(&s_uid_lock);
    return id;
}

/* Écriture complète (gestion EINTR et écritures partielles). */
static void write_all(int fd, const char *buf, int len) {
    int written = 0;
    while (written < len) {
        int ret = (int)write(fd, buf + written, len - written);
        if (ret <= 0) return;
        written += ret;
    }
}

static void handle_client(int fd, RoomManager *rm, UserManager *um) {
    char    buf[600];
    char    response[4096]; /* assez grand pour /list avec 64 users */
    Command cmd;

    /* Nick unique basé sur un compteur, pas sur fd (fd est réutilisé par l'OS). */
    char default_nick[MAX_NAME];
    snprintf(default_nick, MAX_NAME, "user%" PRIu64, next_uid());

    User *u = user_add(um, fd, default_nick);
    if (!u) {
        write_all(fd, "Serveur plein. Reessayez plus tard.\n", 36);
        close(fd);
        return;
    }

    /* Rejoindre la salle "general" par défaut. */
    Room *general = room_get_or_create(rm, "general");
    if (general) {
        room_add_client(general, fd);
        pthread_mutex_lock(&u->lock);
        strncpy(u->current_room, "general", MAX_ROOM - 1);
        pthread_mutex_unlock(&u->lock);
    }

    const char *welcome = "Bienvenue ! Commandes: /nick <pseudo>  /join <salle>"
                          "  /msg <texte>  /pm <user> <texte>  /list  /quit\n";
    write_all(fd, welcome, (int)strlen(welcome));

    int n;
    while ((n = (int)read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        /* Supprimer \r\n ou \n */
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') continue;

        protocol_parse(buf, &cmd);

        switch (cmd.type) {

        /* --- /nick <pseudo> --- */
        case CMD_NICK: {
            if (cmd.arg1[0] == '\0') {
                write_all(fd, "Usage: /nick <pseudo>\n", 22);
                break;
            }
            /* Vérifier l'unicité du nick */
            User *existing = user_find_by_nick(um, cmd.arg1);
            if (existing && existing != u) {
                snprintf(response, sizeof(response),
                         "Pseudo '%s' deja utilise.\n", cmd.arg1);
                write_all(fd, response, (int)strlen(response));
                break;
            }
            pthread_mutex_lock(&u->lock);
            strncpy(u->nick, cmd.arg1, MAX_NAME - 1);
            u->nick[MAX_NAME - 1] = '\0';
            pthread_mutex_unlock(&u->lock);
            snprintf(response, sizeof(response),
                     "Pseudo change en @%s\n", cmd.arg1);
            write_all(fd, response, (int)strlen(response));
            break;
        }

        /* --- /join <salle> --- */
        case CMD_JOIN: {
            if (cmd.arg1[0] == '\0') {
                write_all(fd, "Usage: /join <salle>\n", 21);
                break;
            }
            char old_room_name[MAX_ROOM];
            char nick_copy[MAX_NAME];
            pthread_mutex_lock(&u->lock);
            strncpy(old_room_name, u->current_room, MAX_ROOM - 1);
            old_room_name[MAX_ROOM - 1] = '\0';
            strncpy(nick_copy, u->nick, MAX_NAME - 1);
            nick_copy[MAX_NAME - 1] = '\0';
            pthread_mutex_unlock(&u->lock);

            /* Quitter l'ancienne salle */
            Room *old = room_find(rm, old_room_name);
            if (old) {
                room_remove_client(old, fd);
                snprintf(response, sizeof(response),
                         "*** @%s a quitte #%s ***\n", nick_copy, old_room_name);
                room_broadcast(old, fd, response, (int)strlen(response));
            }

            /* Rejoindre la nouvelle salle */
            Room *r = room_get_or_create(rm, cmd.arg1);
            if (r) {
                room_add_client(r, fd);
                pthread_mutex_lock(&u->lock);
                strncpy(u->current_room, cmd.arg1, MAX_ROOM - 1);
                u->current_room[MAX_ROOM - 1] = '\0';
                pthread_mutex_unlock(&u->lock);

                snprintf(response, sizeof(response),
                         "Vous avez rejoint #%s\n", cmd.arg1);
                write_all(fd, response, (int)strlen(response));

                snprintf(response, sizeof(response),
                         "*** @%s a rejoint #%s ***\n", nick_copy, cmd.arg1);
                room_broadcast(r, fd, response, (int)strlen(response));
            } else {
                write_all(fd, "Impossible de creer la salle (limite atteinte).\n", 48);
            }
            break;
        }

        /* --- /msg <texte> --- */
        case CMD_MSG: {
            char nick_copy[MAX_NAME];
            char room_copy[MAX_ROOM];
            pthread_mutex_lock(&u->lock);
            strncpy(nick_copy, u->nick, MAX_NAME - 1);
            nick_copy[MAX_NAME - 1] = '\0';
            strncpy(room_copy, u->current_room, MAX_ROOM - 1);
            room_copy[MAX_ROOM - 1] = '\0';
            pthread_mutex_unlock(&u->lock);

            /* Utiliser room_find : la salle courante doit déjà exister */
            Room *r = room_find(rm, room_copy);
            if (r) {
                char fmt[700];
                protocol_format(nick_copy, room_copy, cmd.arg2, fmt, sizeof(fmt));
                room_broadcast(r, fd, fmt, (int)strlen(fmt));
            } else {
                write_all(fd, "Vous n'etes dans aucune salle valide. Utilisez /join.\n", 54);
            }
            break;
        }

        /* --- /pm <user> <texte> --- */
        case CMD_PM: {
            if (cmd.arg1[0] == '\0' || cmd.arg2[0] == '\0') {
                write_all(fd, "Usage: /pm <user> <texte>\n", 26);
                break;
            }
            User *target = user_find_by_nick(um, cmd.arg1);
            if (target && target != u) {
                char nick_copy[MAX_NAME];
                pthread_mutex_lock(&u->lock);
                strncpy(nick_copy, u->nick, MAX_NAME - 1);
                nick_copy[MAX_NAME - 1] = '\0';
                pthread_mutex_unlock(&u->lock);

                snprintf(response, sizeof(response),
                         "[PM de @%s] %s\n", nick_copy, cmd.arg2);
                /* Lire le fd de la cible sous son lock */
                pthread_mutex_lock(&target->lock);
                int tfd = target->fd;
                pthread_mutex_unlock(&target->lock);
                write_all(tfd, response, (int)strlen(response));
            } else if (target == u) {
                write_all(fd, "Vous ne pouvez pas vous envoyer un PM.\n", 39);
            } else {
                write_all(fd, "Utilisateur introuvable.\n", 25);
            }
            break;
        }

        /* --- /list --- */
        case CMD_LIST:
            room_list(rm, response, sizeof(response));
            write_all(fd, response, (int)strlen(response));
            user_list(um, response, sizeof(response));
            write_all(fd, response, (int)strlen(response));
            break;

        /* --- /quit --- */
        case CMD_QUIT:
            goto disconnect;

        default:
            write_all(fd, "Commande inconnue. Tapez /list pour de l'aide.\n", 47);
        }
    }

disconnect:
    {
        char nick_copy[MAX_NAME];
        char room_copy[MAX_ROOM];
        pthread_mutex_lock(&u->lock);
        strncpy(nick_copy, u->nick, MAX_NAME - 1);
        nick_copy[MAX_NAME - 1] = '\0';
        strncpy(room_copy, u->current_room, MAX_ROOM - 1);
        room_copy[MAX_ROOM - 1] = '\0';
        pthread_mutex_unlock(&u->lock);

        Room *r = room_find(rm, room_copy);
        if (r) {
            room_remove_client(r, fd);
            snprintf(response, sizeof(response),
                     "*** @%s a quitte #%s ***\n", nick_copy, room_copy);
            room_broadcast(r, fd, response, (int)strlen(response));
        }
        user_remove(um, fd);
        close(fd);
        printf("[server] fd=%d (@%s) deconnecte\n", fd, nick_copy);
    }
}

/* ---- Thread worker ---- */
static void *worker_fn(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;
    while (tp->running) {
        int fd = queue_pop(&tp->task_queue);
        if (fd == -1) break;
        printf("[worker] Prise en charge fd=%d\n", fd);
        handle_client(fd, tp->room_mgr, tp->user_mgr);
    }
    return NULL;
}

/* ---- API publique ---- */
void thread_pool_init(ThreadPool *tp, RoomManager *rm, UserManager *um) {
    tp->room_mgr = rm;
    tp->user_mgr = um;
    tp->running  = 1;
    queue_init(&tp->task_queue);
    for (int i = 0; i < POOL_SIZE; i++)
        pthread_create(&tp->threads[i], NULL, worker_fn, tp);
    printf("[pool] %d workers demarres\n", POOL_SIZE);
}

void thread_pool_submit(ThreadPool *tp, int client_fd) {
    queue_push(&tp->task_queue, client_fd);
}

void thread_pool_destroy(ThreadPool *tp) {
    tp->running = 0;
    /* Envoyer un fd sentinel par worker pour les débloquer */
    for (int i = 0; i < POOL_SIZE; i++)
        queue_push(&tp->task_queue, -1);
    for (int i = 0; i < POOL_SIZE; i++)
        pthread_join(tp->threads[i], NULL);
    queue_destroy(&tp->task_queue);
    printf("[pool] Tous les workers arretes.\n");
}
