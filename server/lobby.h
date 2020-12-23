#ifndef LOBBY_H
#define LOBBY_H

#include <stdbool.h>


#define MAX_LOBBIES 32

typedef struct Client {
    int sfd;
    char ip[32];
    int port;
} Client;

struct Lobby;

typedef struct Player {
    Client client;
    char name[32];
    int index;
    bool leave;
    struct Lobby* lobby;
} Player;

typedef struct Lobby {
    int index;
    Player players[4];
    int players_nb;
} Lobby;

void broadcast(Lobby* lobby, char* msg, unsigned int size);


#endif//LOBBY_H
