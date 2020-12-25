#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>

#include "common/typedefs.h"
#include "server/lobby.h"

void Lobby_lock(Lobby* lobby, Player* player) {
    pthread_mutex_lock(&lobby->mutex_players);
    lobby->locked = true;
    printf("[LOCK] Lobby %d is locked by %s\n\n", lobby->index, player == NULL ? "lobby" : player->name);
}

void Lobby_unlock(Lobby* lobby, Player* player) {
    printf("[LOCK] Lobby %d is unlocked\n\n", lobby->index);
    pthread_mutex_unlock(&lobby->mutex_players);
    lobby->locked = false;
}

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size) {
    printf("[INFO] Broadcasting message to %d players in lobby %d : \"%s\"\n", lobby->players_nb, lobby->index, msg);
    for (int i = 0; i < lobby->players_nb; ++i) {
        if (!lobby->players[i]->leave)
            send_msg(lobby->players[i], msg, sizeof(char)*size);
    }
}

inline void Lobby_waitAcks(Lobby* lobby) {
    bool acks[4] = {false, false, false, false};
    int acks_nb = 0;
    for (int timeout = 100000; timeout > 0; --timeout) {
        for (int i = 0; i < lobby->players_nb; ++i) {
            Lobby_unlock(lobby, NULL);
            //pthread_mutex_unlock(&lobby->mutex_players);
            sched_yield();
            //pthread_mutex_lock(&lobby->mutex_players);
            Lobby_lock(lobby, NULL);
            if (acks[i] == false && lobby->players[i]->client.ack)  {
                printf("[BROAD] Player %d:%s acked\n", lobby->index, lobby->players[i]->name);
                acks[i] = true;
                acks_nb++;
            }
        }
        if (acks_nb == lobby->players_nb)
            break;
        if (timeout == 1) {
            printf("[BROAD] Warning : Ack timeout\n");
            for (int i = 0; i < lobby->players_nb; ++i)
                if (acks[i] == false)
                    printf("     > Player %d:%s did not ack\n",lobby->index, lobby->players[i]->name);
        }
    }
    for (int i = 0; i < lobby->players_nb; ++i) {
        lobby->players[i]->client.ack = false; 
    }
    if (!lobby->locked)
        Lobby_unlock(lobby, NULL);
}

void* manage_lobby_thread(void* lobby) {

    Lobby* this_lobby = (Lobby*)lobby;
    printf("[INFO] Started thread for lobby %d (%ld) \n\n", this_lobby->index, pthread_self());

    MsgQueue* msg_queue = &this_lobby->queue;
    MsgQueue_Clear(msg_queue);
    
    // broadcast GameStart message to lobby
    char buffer[256];
    buffer[0] = (char)GameStart;
    int current_i = 1;
    for (int i = 0; i < 4; ++i) {
        int name_len = strlen(this_lobby->players[i]->name);
        buffer[current_i] = name_len+'0';
        strcpy(&buffer[current_i+1], this_lobby->players[i]->name);
        current_i += name_len+1;
    }
    MsgQueue_Append(msg_queue, buffer, current_i+1, -1);

    this_lobby->send_next = true;
    bool close_lobby = false;

    while (1) {
        if (this_lobby->players_nb == 0)
            close_lobby = true;
        if (close_lobby)
            break;

        if (this_lobby->send_next) {
            if (msg_queue->size > 0) {
                MsgNode* msg = MsgQueue_Front(msg_queue);

                if (msg->dest == -1) {
                    printf("[LOCK] Lobby %d waiting for Mutex lock\n\n", this_lobby->index);
                    Lobby_lock(this_lobby, NULL);

                    Lobby_broadcast(this_lobby, msg->msg, msg->size);
                    Lobby_waitAcks(this_lobby);

                    Lobby_unlock(this_lobby, NULL);
                }
                else {
                    Player* player = this_lobby->players[msg->dest];
                    send_msg(player, msg->msg, msg->size);
                }

                MsgQueue_Pop(msg_queue);
                this_lobby->send_next = false;
            }
            else {
                sched_yield();
            }
        }
        else {
            sched_yield();
        }
    }

    printf("[INFO] Closing thread for lobby %d (%ld) \n\n", this_lobby->index, pthread_self());

    MsgQueue_Clear(msg_queue);
    pthread_exit(NULL);

}

void* manage_player_thread(void* player) {
    Player* this_player = (Player*)player;
    Lobby* this_lobby = this_player->lobby;
    
    char buffer[256];
    int msg_size;
    
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        if (this_player->leave) {
            printf("[INFO] Player %s leaving lobby %d\n\n", this_player->name, this_lobby->index);
            break;
        }
        msg_size = recv_msg(this_player, buffer, sizeof(buffer));
        
        if (this_player->leave) {
            printf("[INFO] Player %s leaving lobby %d\n\n", this_player->name, this_lobby->index);
            break;
        }
        if (msg_size <= 0) {
            
            printf("[INFO] Failed to read from player %s:%u\n[INFO] Closing connection with player %s from lobby %d.\n\n", this_player->client.ip, this_player->client.port, this_player->name, this_lobby->index);
            if (this_lobby->players_nb == 0)
                break;
                
            printf("[LOCK] Player %s waiting for Mutex lock\n\n", this_player->name);
            Lobby_lock(this_lobby, this_player);

            // player was the only on in lobby
            if (this_lobby->players_nb == 1) {
                this_lobby->players_nb--;
            }
            // game not started yet, tell other players in lobby that this_player has quit
            else if (1 < this_lobby->players_nb && this_lobby->players_nb < 4) {
                // rearange players index
                for (int i = this_player->index; i < 2; ++i) {
                    this_lobby->players[i] = this_lobby->players[i+1];
                    this_player->index++;
                }
                this_lobby->players_nb--;
                for (int i = this_lobby->players_nb; i < 4; ++i) {
                    this_lobby->players[i] = NULL;
                }
                
                buffer[0] = WaitingPlayers;
                buffer[1] = this_lobby->players_nb+'0';
                buffer[2] = '\0';
                Lobby_broadcast(this_lobby, buffer, 3);
                Lobby_waitAcks(this_lobby);
                printf("     > Player %s quit, %d players remaning in lobby %d\n\n", this_player->name, this_lobby->players_nb, this_lobby->index);
            }
            // if game already started, just make everyone quit
            else if (this_lobby->players_nb == 4) {
                buffer[0] = (char)QuitLobby;
                buffer[1] = '\0';
                this_player->leave = true;
                printf("     > %s from lobby %d quitted game !\n\n", this_player->name, this_lobby->index);
                Lobby_broadcast(this_lobby, buffer, 2);
                for (int i = 0; i < 4; ++i) {
                    if (this_lobby->players[i] != NULL)
                        this_lobby->players[i]->leave = true;
                }
                this_lobby->players_nb = 0;
                *this_lobby->lobby_states &= 0 << this_lobby->index ;
            }
            Lobby_unlock(this_lobby, this_player);
            break;
        }
        this_player->client.ack = (strcmp(buffer, "ack") == 0);
    }
    close(this_player->client.sfd);
    if (this_player != NULL)
        free(this_player);
    pthread_exit(NULL);
}

int send_msg(Player* player, void* data, int size) {
    int r = write(player->client.sfd, data, size);
    if (r < 0) {
        printf("[INFO] Failed to send message to %s (lobby %d): \"%s\"\n\n", player->name, player->lobby->index, (char*)data);
    }
    else
        printf("[%s:%d] %d:%s < \"%s\" = %d bytes\n\n", player->client.ip, player->client.port, player->lobby->index, player->name, (char*)data, r);
    return r;
}

int recv_msg(Player* player, void* buffer, int size) {
    int r = read(player->client.sfd, buffer, size);
    if (r < 0)
        printf("[INFO] Failed to receive message from %s [%s:%d]\n\n", player->name, player->client.ip, player->client.port);
    else
        printf("[%s:%d] %d:%s > \"%s\" = %d bytes\n\n", player->client.ip, player->client.port, player->lobby->index, player->name, (char*)buffer, r);
    return r;
}
