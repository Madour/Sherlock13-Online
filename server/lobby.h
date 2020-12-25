#ifndef LOBBY_H
#define LOBBY_H

#include <stdbool.h>
#include "server/player.h"
#include "server/msg_queue.h"


#define MAX_LOBBIES 32

typedef struct Lobby {
    int index;
    Player* players[4];
    int players_nb;

    MsgQueue queue;
    bool send_next;

    pthread_mutex_t mutex_players;
    bool locked;

    // global lobbies state
    int* lobby_states;
} Lobby;


void Lobby_lock(Lobby* lobby, Player* player);
void Lobby_unlock(Lobby* lobby, Player* player);

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size);

void Lobby_waitAcks(Lobby* lobby);

void* manage_lobby_thread(void* lobby);

void* manage_player_thread(void* player);

int send_msg(Player* player, void* data, int size);
int recv_msg(Player* player, void* buffer, int size);


#endif//LOBBY_H
