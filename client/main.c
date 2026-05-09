#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>

#define BUF_SIZE 700

static int sock_fd;
static volatile int connected = 1;

/* Thread de réception : affiche tout ce que le serveur envoie. */
static void *recv_thread(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];
    int n;
    while (connected && (n = (int)read(sock_fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        fflush(stdout);
    }
    connected = 0;
    printf("\n[client] Deconnecte du serveur.\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip   = argv[1];
    int         port = atoi(argv[2]);

    /* Ignorer SIGPIPE : géré par les retours de send() */
    signal(SIGPIPE, SIG_IGN);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Adresse IP invalide : %s\n", ip);
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("[client] Connecte a %s:%d\n", ip, port);
    printf("Commandes: /nick <pseudo>  /join <salle>  /msg <texte>\n");
    printf("           /pm <user> <texte>  /list  /quit\n\n");

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);

    char line[BUF_SIZE];
    while (connected && fgets(line, sizeof(line), stdin)) {
        if (!connected) break;

        int len = (int)strlen(line);
        if (len == 0) continue;

        /* Écriture complète */
        int written = 0;
        while (written < len) {
            int ret = (int)send(sock_fd, line + written, len - written, 0);
            if (ret <= 0) { connected = 0; break; }
            written += ret;
        }

        if (strncmp(line, "/quit", 5) == 0) break;
    }

    connected = 0;
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    pthread_join(tid, NULL);
    return 0;
}
