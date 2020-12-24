#include "server/player.h"
#include <string.h>
#include <stdio.h>


void Player_Copy(Player* dest, Player* src) {
    printf("Copying player %d-%s to %d-%s\n\n", src->index, src->name, dest->index, dest->name);
    dest->index = src->index;
    strcpy(dest->name, src->name);
    
    dest->lobby = src->lobby;
    dest->leave = src->leave;

    dest->client.sfd = src->client.sfd;
    strcpy(dest->client.ip, src->client.ip);
    dest->client.port = src->client.port;
    dest->client.ack = src->client.ack;
}
