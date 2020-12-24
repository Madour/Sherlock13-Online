#include "server/player.h"
#include <string.h>

void Player_Copy(Player* dest, Player* src) {
    dest->index = src->index;
    strcpy(dest->name, src->name);
    dest->lobby = src->lobby;

    dest->wait_msg = src->wait_msg;
    dest->leave = src->leave;

    dest->client.sfd = src->client.sfd;
    strcpy(dest->client.ip, src->client.ip);
    dest->client.port = src->client.port;
    dest->client.ack = src->client.ack;
}
