#include <stdio.h>
#include <string.h>
#include "protocol.h"

int protocol_parse(const char *raw, Command *out) {
    memset(out, 0, sizeof(Command));
    out->type = CMD_UNKNOWN;

    if (strncmp(raw, "/join ", 6) == 0) {
        out->type = CMD_JOIN;
        sscanf(raw + 6, "%63s", out->arg1);
    } else if (strncmp(raw, "/msg ", 5) == 0) {
        out->type = CMD_MSG;
        strncpy(out->arg2, raw + 5, sizeof(out->arg2) - 1);
        out->arg2[sizeof(out->arg2) - 1] = '\0';
    } else if (strncmp(raw, "/list", 5) == 0) {
        out->type = CMD_LIST;
    } else if (strncmp(raw, "/pm ", 4) == 0) {
        out->type = CMD_PM;
        sscanf(raw + 4, "%63s %511[^\n]", out->arg1, out->arg2);
    } else if (strncmp(raw, "/quit", 5) == 0) {
        out->type = CMD_QUIT;
    } else if (strncmp(raw, "/nick ", 6) == 0) {
        out->type = CMD_NICK;
        sscanf(raw + 6, "%63s", out->arg1);
    }

    return out->type;
}

void protocol_format(const char *sender, const char *room,
                     const char *body, char *out, int outlen) {
    snprintf(out, outlen, "[%s][%s] %s\n", room, sender, body);
}
