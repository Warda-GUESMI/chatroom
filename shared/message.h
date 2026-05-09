#ifndef MESSAGE_H
#define MESSAGE_H

#include <time.h>

#define MAX_NAME   32
#define MAX_ROOM   32
#define MAX_BODY   512

typedef struct {
    char   sender[MAX_NAME];
    char   room[MAX_ROOM];
    char   body[MAX_BODY];
    time_t timestamp;
} Message;

#endif
