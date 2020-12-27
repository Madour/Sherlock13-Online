#ifndef LOBBY_H
#define LOBBY_H

#include <stdbool.h>
#include "server/msg_queue.h"


#define MAX_LOBBIES 32


typedef struct Client {
    int sfd;
    char ip[32];
    int port;
    bool ack;
} Client;

struct Lobby;

typedef struct Player {
    Client client;
    int index;
    char name[32];
    int cards[3];
    int items_count[8];

    bool leave;
    struct Lobby* lobby;
} Player;

typedef struct Lobby {
    int index;
    Player* players[4];
    int players_nb;

    bool game_ended;
    int suspect;
    int turn;
    int penalities;

    MsgQueue queue;
    pthread_cond_t send_next;

    pthread_mutex_t mutex;
    bool locked;

    // global lobbies state
    int* lobby_states;
} Lobby;

void Lobby_reset(Lobby* lobby);

void Lobby_lock(Lobby* lobby, Player* player);
void Lobby_unlock(Lobby* lobby, Player* player);

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size);

void Lobby_waitAcks(Lobby* lobby);

void* manage_lobby_thread(void* lobby);

void* manage_player_thread(void* player);

int send_msg(Player* player, void* buffer, int size);
int recv_msg(Player* player, void* buffer, int size);


#endif//LOBBY_H
