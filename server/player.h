#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

typedef struct Client {
    int sfd;
    char ip[32];
    int port;
    bool ack;
} Client;

struct Lobby;

typedef struct Player {
    Client client;
    char name[32];
    int index;
    bool leave;
    struct Lobby* lobby;
} Player;


void Player_copy(Player* dest, Player* src);

#endif//PLAYER_H