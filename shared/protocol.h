#ifndef PROTOCOL_H
#define PROTOCOL_H

#define CMD_UNKNOWN -1
#define CMD_JOIN     0
#define CMD_MSG      1
#define CMD_LIST     2
#define CMD_PM       3
#define CMD_QUIT     4
#define CMD_NICK     5

typedef struct {
    int  type;
    char arg1[64];
    char arg2[512];
} Command;

int  protocol_parse(const char *raw, Command *out);
void protocol_format(const char *sender, const char *room,
                     const char *body, char *out, int outlen);

#endif
