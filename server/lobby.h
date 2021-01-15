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

typedef struct Client {
    int sfd;
    char ip[32];
    int port;
    bool ack;
} Client;

struct Lobby;

typedef struct Player {
    Client client;

    int index;  // player index in the Lobby
    char name[32];
    int cards[3];
    int items_count[8];

    bool leave;
    struct Lobby* lobby;
} Player;

enum LobbyState {
    LobbyStateWaiting,
    LobbyStateGameStarted,
    LobbyStateGameEnded,
    LobbyStateReplay,
};

typedef struct Lobby {
    int index;
    Player* players[4];
    int players_nb;

    int turn;
    int suspect;
    int penalities; // when a player does a wrong guess, 
                    // their next turn will be skipped

    enum LobbyState state;
    bool available;
    bool quit;  // is true when server shutdown only

    MsgQueue queue;
    pthread_cond_t cond;

    pthread_t thread;
    pthread_mutex_t mutex;
    bool locked;
} Lobby;

// resets Lobby data, called only once at server launch
void Lobby_reset(Lobby* lobby);

// inserts a new player in the lobby (doesnt check if lobby is already full)
void Lobby_addNewPlayer(Lobby* lobby, Player* player);

// shuffles and distributes cards and creates game start messages
void Lobby_startGame(Lobby* lobby);

/**
 * Sends a cond signal to lobby thread. Lobby will then
 * send all the messages in its msg queue to the clients.
 */
void Lobby_sendMsgs(Lobby* lobby, const char* name);

// locks Lobby mutex
void Lobby_lock(Lobby* lobby, const char* name);

// unlocks Lobby mutex
void Lobby_unlock(Lobby* lobby, const char* name);

// broadcasts a message to all clients in lobby
void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size);

// usually called after broadcast, creates a short delay to let clients ack
void Lobby_waitAcks(Lobby* lobby);

// prints player informations
void Lobby_printPlayers(Lobby* lobby);

/**
 * Lobby thread function. Wait for the cond signal before sending
 * all messages in the msg queue to their targets. This thread will
 * always have the lobby mutex under control, only releasing it
 * when waiting for a signal.
 */ 
void* lobby_thread_func(void* lobby);

/**
 * Player thread function. Handles receving message from one player.
 * Decode the message and execute the correct actions.
 * Communicate with loby thread with the dedicated cond signal.
 */
void* player_thread_func(void* player);


// Utility functions for sending and receiving messages to/from clients
int send_msg(Player* player, void* buffer, int size);
int recv_msg(Player* player, void* buffer, int size);


#endif//LOBBY_H
