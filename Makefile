CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -g -pthread -I./shared
LDFLAGS = -pthread

SRV_SRC = server/main.c server/socket.c server/thread_pool.c \
          server/queue.c shared/room.c shared/user.c shared/protocol.c

CLT_SRC = client/main.c shared/protocol.c

all: server client

server: $(SRV_SRC)
	$(CC) $(CFLAGS) -o server/chatserver $(SRV_SRC) $(LDFLAGS)

client: $(CLT_SRC)
	$(CC) $(CFLAGS) -o client/chatclient $(CLT_SRC) $(LDFLAGS)

clean:
	rm -f server/chatserver client/chatclient

valgrind-server: server
	valgrind --tool=helgrind --track-lockorders=yes ./server/chatserver 9090

.PHONY: all clean valgrind-server
