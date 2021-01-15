/**
 * @file lobby.h
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

#ifndef LOBBY_H
#define LOBBY_H

#include <stdbool.h>
#include "server/msg_queue.h"


#define MAX_LOBBIES (sizeof(int)*8)

enum LobbyState {
    LobbyStateWaiting,
    LobbyStateGameStarted,
    LobbyStateGameEnded,
    LobbyStateReplay,
};

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

    int turn;
    int suspect;
    // when a player does a wrong guess, their next turn will be skipped
    int penalities;

    enum LobbyState state;
    bool available;
    bool quit;

    MsgQueue queue;
    pthread_cond_t cond;

    pthread_t thread;
    pthread_mutex_t mutex;
    bool locked;
} Lobby;

// reset Lobby data, called only once at server launch
void Lobby_reset(Lobby* lobby);

void Lobby_addNewPlayer(Lobby* lobby, Player* player);

void Lobby_startGame(Lobby* lobby);

/**
 * Send a cond signal to lobby thread
 * Lobby will send all the messages in its queue to the clients
 */
void Lobby_sendMsgs(Lobby* lobby, const char* name);

// lock unlock Lobby mutex
void Lobby_lock(Lobby* lobby, const char* name);
void Lobby_unlock(Lobby* lobby, const char* name);

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size);
void Lobby_waitAcks(Lobby* lobby);

void Lobby_printPlayers(Lobby* lobby);

void* manage_lobby_thread(void* lobby);
void* manage_player_thread(void* player);


// Utility functions for sending and receiving messages to/from clients
int send_msg(Player* player, void* buffer, int size);
int recv_msg(Player* player, void* buffer, int size);


#endif//LOBBY_H
