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

    int suspect;
    int turn;
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

void Lobby_reset(Lobby* lobby);

void Lobby_addNewPlayer(Lobby* lobby, Player* player);

void Lobby_startGame(Lobby* lobby);

void Lobby_sendMsgs(Lobby* lobby, Player* player);

void Lobby_lock(Lobby* lobby, Player* player);
void Lobby_unlock(Lobby* lobby, Player* player);

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size);
void Lobby_waitAcks(Lobby* lobby);

void Lobby_printPlayers(Lobby* lobby);

void* manage_lobby_thread(void* lobby);
void* manage_player_thread(void* player);

int send_msg(Player* player, void* buffer, int size);
int recv_msg(Player* player, void* buffer, int size);


#endif//LOBBY_H
