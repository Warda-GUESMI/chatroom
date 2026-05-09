#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include "socket.h"
#include "thread_pool.h"
#include "../shared/room.h"
#include "../shared/user.h"

static volatile int running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Port invalide : %s\n", argv[1]);
        return 1;
    }

    signal(SIGINT,  handle_sigint);
    signal(SIGPIPE, SIG_IGN); /* Ignorer SIGPIPE : géré par les retours de write() */

    RoomManager rm;
    UserManager um;
    room_manager_init(&rm);
    user_manager_init(&um);
    room_get_or_create(&rm, "general"); /* salle par défaut */

    ThreadPool pool;
    thread_pool_init(&pool, &rm, &um);

    int server_fd = server_socket_create(port);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    printf("[server] En attente de connexions (Ctrl+C pour arreter)...\n");

    while (running) {
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (!running) break;
            perror("accept");
            continue;
        }
        printf("[server] Nouveau client fd=%d depuis %s:%d\n",
               client_fd,
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        thread_pool_submit(&pool, client_fd);
    }

    server_socket_close(server_fd);
    thread_pool_destroy(&pool);
    printf("[server] Arrete proprement.\n");
    return 0;
}
