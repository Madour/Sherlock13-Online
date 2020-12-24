#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>

#include "common/typedefs.h"
#include "server/lobby.h"

void Lobby_Broadcast(Lobby* lobby, char* msg, unsigned int size) {
    printf("[INFO] Broadcasting message to lobby %d : \"%s\"\n", lobby->index, msg);
    for (int i = 0; i < lobby->players_nb; ++i) {
        lobby->players[i].wait_msg = true;
    }

    for (int i = 0; i < lobby->players_nb; ++i) {
        send_msg(&lobby->players[i], msg, sizeof(char)*size);
    }
    printf("\n");
}

void Lobby_WaitAcks(Lobby* lobby) {
    printf("[INFO] Waiting acks\n");
    for (int timeout = 100000; timeout > 0; --timeout) {
        int acks = 0;
        for (int i = 0; i < lobby->players_nb; ++i) {
            sched_yield();
            if (lobby->players[i].client.ack) 
                acks++;
        }
        if (acks == lobby->players_nb)
            break;
        if (timeout == 1)
            printf("[WARNING] Ack timeout after broadcast\n");
    }
    for (int i = 0; i < 4; ++i) {
        lobby->players[i].wait_msg = false;
        lobby->players[i].client.ack = false; 
    }
}

void* manage_lobby_thread(void* lobby) {

    Lobby* this_lobby = (Lobby*)lobby;
    printf("[INFO] Started thread for lobby %d (%ld) \n\n", this_lobby->index, pthread_self());

    MsgQueue* msg_queue = &this_lobby->queue;
    MsgQueue_Clear(msg_queue);
    
    // broadcast GameStart message to lobby
    char buffer[32];
    buffer[0] = (char)GameStart;
    int current_i = 1;
    for (int i = 0; i < 4; ++i) {
        int name_len = strlen(this_lobby->players[i].name);
        buffer[current_i] = name_len+'0';
        strcpy(&buffer[current_i+1], this_lobby->players[i].name);
        current_i += name_len+1;
    }
    MsgQueue_Append(msg_queue, buffer, current_i+1, -1);

    this_lobby->send_next = true;
    bool close_lobby = false;

    while (1) {
        for (int i = 0; i < 4; ++i)
            if (this_lobby->players[i].leave)
                close_lobby = true;
        if (close_lobby)
            break;

        if (this_lobby->send_next) {
            if (msg_queue->size > 0) {
                MsgNode* msg = MsgQueue_Front(msg_queue);

                if (msg->dest == -1) {
                    Lobby_Broadcast(this_lobby, msg->msg, msg->size);
                    Lobby_WaitAcks(this_lobby);
                }
                else {
                    Player* player = &this_lobby->players[msg->dest];
                    player->wait_msg = true;
                    write(player->client.sfd, msg->msg, msg->size);
                    printf("[%s:%d] %s < %d : \"%s\"", player->client.ip, player->client.port, player->name, this_lobby->index, msg->msg);
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
        /*
        if (!this_player->wait_msg)
            sched_yield();
        */
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
            printf("[INFO] Failed to read from client %s:%u\n", this_player->client.ip, this_player->client.port);
            printf("[INFO] Closing connection with player %s from lobby %d.\n\n", this_player->name, this_lobby->index);

            // player was the only on in lobby
            if (this_lobby->players_nb == 1) {
                this_lobby->players_nb--;
            }
            // game not started yet, tell other players in lobby that this_player has quit
            else if (1 < this_lobby->players_nb && this_lobby->players_nb < 4) {
                // rearange players index
                for (int i = this_player->index; i < 2; ++i) {
                    Player_Copy(&this_lobby->players[i], &this_lobby->players[i+1]);
                }
                this_lobby->players_nb--;
                
                buffer[0] = WaitingPlayers;
                buffer[1] = this_lobby->players_nb+'0';
                buffer[2] = '\0';
                Lobby_Broadcast(this_lobby, buffer, 3);
                Lobby_WaitAcks(this_lobby);
                
            }
            // if game already started, just make everyone quit
            else {
                
                buffer[0] = (char)QuitLobby;
                buffer[1] = '\0';
                Lobby_Broadcast(this_lobby, buffer, 2);
                for (int i = 0; i < 4; ++i) {
                    this_lobby->players[i].leave = true;
                }
                this_lobby->players_nb = 0;
                *this_lobby->lobby_states &= 0 << this_lobby->index ;
            }
            break;
        }
        if (strcmp(buffer, "ack") == 0) {
            this_player->client.ack = true;
        }
        this_player->wait_msg = false;
    }
    this_player->index = -1;
    close(this_player->client.sfd);
    pthread_exit(NULL);
}

int send_msg(Player* player, void* data, int size) {
    int r = write(player->client.sfd, data, size);
    if (r < 0) {
        printf("[INFO] Failed to send message to %s (lobby %d): \"%s\"\n\n", player->name, player->lobby->index, (char*)data);
    }
    else
        printf("[%s:%d] %d:%s < \"%s\"\n\n", player->client.ip, player->client.port, player->lobby->index, player->name, (char*)data);
    return r;
}

int recv_msg(Player* player, void* buffer, int size) {
    int r = read(player->client.sfd, buffer, size);
    if (r < 0)
        printf("[INFO] Failed to receive message from %s [%s:%d]\n\n", player->name, player->client.ip, player->client.port);
    else
        printf("[%s:%d] %d:%s > \"%s\"\n\n", player->client.ip, player->client.port, player->lobby->index, player->name, (char*)buffer);
    return r;
}
