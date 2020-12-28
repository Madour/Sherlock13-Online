#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>

#include "common/typedefs.h"
#include "server/lobby.h"

void Lobby_reset(Lobby* lobby) {
    for (int p = 0; p < 4; ++p) {
        /*if (lobby->players[p] != NULL)
            free(lobby->players[p]);*/
        lobby->players[p] = NULL;
    }

    lobby->game_ended = false;
    lobby->suspect = 0;
    lobby->turn = 0;
    lobby->penalities = 0;

    MsgQueue_init(&lobby->queue);
    pthread_cond_init(&lobby->send_next, NULL);

    pthread_mutex_init(&lobby->mutex, NULL);
    lobby->locked = false;
}

void Lobby_lock(Lobby* lobby, Player* player) {
    if (player != NULL)
        deb_log("[LOCK] Lobby %d : Player %s waiting for Mutex lock\n", lobby->index, player->name);
    else
        deb_log("[LOCK] Lobby %d : Waiting for Mutex lock\n", lobby->index);
    pthread_mutex_lock(&lobby->mutex);
    lobby->locked = true;
    deb_log("[LOCK] Lobby %d is locked by %s\n", lobby->index, player == NULL ? "lobby" : player->name);
}

void Lobby_unlock(Lobby* lobby, Player* player) {
    deb_log("[LOCK] Lobby %d is unlocked by %s\n", lobby->index, player == NULL ? "lobby" : player->name);
    pthread_mutex_unlock(&lobby->mutex);
    lobby->locked = false;
}

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size) {
    deb_log("[INFO] Lobby %d : Broadcasting message to %d players : \"%s\"\n", lobby->index, lobby->players_nb, msg);
    for (int i = 0; i < lobby->players_nb; ++i) {
        if (lobby->players[i] != NULL)
            if (!lobby->players[i]->leave)
                send_msg(lobby->players[i], msg, sizeof(char)*size);
    }
}

inline void Lobby_waitAcks(Lobby* lobby) {
    bool acks[4] = {false, false, false, false};
    int acks_nb = 0;
    for (int timeout = 10000; timeout > 0; --timeout) {
        for (int i = 0; i < lobby->players_nb; ++i) {
            if (lobby->players[i] != NULL) {
                pthread_mutex_unlock(&lobby->mutex);
                sched_yield();
                pthread_mutex_lock(&lobby->mutex);
                if (acks[i] == false && lobby->players[i]->client.ack)  {
                    deb_log("[BROAD] Player %d:%s acked\n", lobby->index, lobby->players[i]->name);
                    acks[i] = true;
                    acks_nb++;
                }
            }
            
        }
        if (acks_nb == lobby->players_nb)
            break;
        if (timeout == 1) {
            deb_log("[BROAD] Warning : Ack timeout\n");
            for (int i = 0; i < lobby->players_nb; ++i) {
                if (lobby->players[i] != NULL &&  acks[i] == false)
                    deb_log("[BROAD] Lobby %d: Player %s did not ack\n",lobby->index, lobby->players[i]->name);
            }
        }
    }
    for (int i = 0; i < lobby->players_nb; ++i) {
        if (lobby->players[i] != NULL)
            lobby->players[i]->client.ack = false; 
    }
    if (!lobby->locked)
        Lobby_unlock(lobby, NULL);
}

void* manage_lobby_thread(void* lobby) {

    Lobby* this_lobby = (Lobby*)lobby;
    deb_log("[INFO] Lobby %d : Started thread (%ld) \n", this_lobby->index, pthread_self());

    MsgQueue* msg_queue = &this_lobby->queue;
    MsgQueue_clear(msg_queue);
    
    // broadcast GameStart message to lobby
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = (char)GameStart;
    int current_i = 1;
    for (int i = 0; i < 4; ++i) {
        int name_len = strlen(this_lobby->players[i]->name);
        buffer[current_i] = name_len+'0';
        strcpy(&buffer[current_i+1], this_lobby->players[i]->name);
        current_i += name_len+1;
    }
    MsgQueue_append(msg_queue, buffer, current_i+1, -1);

    printf("     > Lobby %d : Game started ! Distributing cards to players :\n\n", this_lobby->index);
    // filling players cards and adding the message to the queue
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = (char)DistribCards;
    int taken_cards = 0;
    for (int i = 0; i < 4; ++i) {
        printf("         > Player %d:%s cards : \n", this_lobby->index, this_lobby->players[i]->name);
        for (int j = 0; j < 3; ++j) {
            int card;
            bool found = false;
            do {
                card = rand()%13;
                if ( ((taken_cards>>card)&1) == 0 ) {
                    this_lobby->players[i]->cards[j] = card;
                    for (int item=0; item<3; ++item) {
                        int chara_item = DATA.characters_items[card][item];
                        if (chara_item != -1)
                            this_lobby->players[i]->items_count[chara_item] += 1;
                    }
                    taken_cards |= 1 << card;
                    found = true;
                    printf("           - %2d = %s\n", card, DATA.characters_names[card]);
                }
            } while(!found);
            buffer[1+j] = (char)(card+1);
        }
        buffer[4] = '\0';
        MsgQueue_append(msg_queue, buffer, 5, i);
        printf("\n");
    }
    for (int i = 0; i < 13; ++i)
        if ( ((taken_cards >> i)&1) == 0 )
            this_lobby->suspect = i;

    printf("     > Lobby %d : Suspect is \"%s\" (%d)\n\n", this_lobby->index, DATA.characters_names[this_lobby->suspect], this_lobby->suspect);

    bool close_lobby = false;
    bool first_msg = true;

    while (1) {
        if (this_lobby->players_nb == 0)
            close_lobby = true;
        if (close_lobby)
            break;

        Lobby_lock(this_lobby, NULL);
        if (!first_msg) {
            deb_log("[SIGN] Lobby %d waiting signal for sending. Lock released \n", this_lobby->index);
            pthread_cond_wait(&this_lobby->send_next, &this_lobby->mutex);
        }
        else
            first_msg = false;

            
        while (msg_queue->size > 0) {
            MsgNode* msg = MsgQueue_front(msg_queue);

            if (msg->dest == -1) {
                Lobby_broadcast(this_lobby, msg->msg, msg->size);
                if (msg->msg[0] != (char)QuitLobby)
                    Lobby_waitAcks(this_lobby);
            }
            else {
                Player* player = this_lobby->players[msg->dest];
                send_msg(player, msg->msg, msg->size);
            }

            if (msg->msg[0] == (char)QuitLobby) {
                close_lobby = true;
                
            }
            MsgQueue_pop(msg_queue);
        }
        
        if (close_lobby) {
            for (int i = 0; i < 4; ++i)
                if (this_lobby->players[i] != NULL)
                    this_lobby->players[i]->leave = true;
        }
        Lobby_unlock(this_lobby, NULL);
    }

    deb_log("[INFO] Lobby %d : Closing thread (%ld) \n", this_lobby->index, pthread_self());
    this_lobby->players_nb = 0;
    this_lobby->game_ended = false;
    *this_lobby->lobby_states &= 0 << this_lobby->index ;
    MsgQueue_clear(msg_queue);
    pthread_exit(NULL);
}

void* manage_player_thread(void* player) {
    Player* this_player = (Player*)player;
    Lobby* this_lobby = this_player->lobby;
    
    char buffer[256];
    char tmp[64];
    memset(tmp, 0, sizeof(tmp));
    int msg_size;
    
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        if (this_player->leave) {
            printf("     > Lobby %d : Player %s is leaving.\n\n", this_lobby->index, this_player->name);
            break;
        }
        msg_size = recv_msg(this_player, buffer, sizeof(buffer));
        
        if (msg_size <= 0) {
            deb_log("[INFO] Failed to read from player %s:%u\n[INFO] Closing connection with player %s from lobby %d.\n", this_player->client.ip, this_player->client.port, this_player->name, this_lobby->index);
            if (this_lobby->players_nb == 0 || this_player->leave) {
                printf("     > Lobby %d : Player %s is leaving.\n\n", this_lobby->index, this_player->name);
                break;
            }
                
            Lobby_lock(this_lobby, this_player);

            if (this_player->leave) {
                printf("     > Lobby %d : Player %s is leaving.\n\n", this_lobby->index, this_player->name);
                Lobby_unlock(this_lobby, this_player);
                break;
            }
            
            if (!this_lobby->game_ended) {
                // player was the only on in lobby
                if (this_lobby->players_nb == 1) {
                    this_lobby->players_nb--;
                }
                // a player qui while in game
                else if (this_lobby->players_nb == 4) {
                    printf("     > Lobby %d : Player %s quitted game !\n\n", this_lobby->index, this_player->name);
                    // create Quit message
                    tmp[0] = (char)QuitLobby;
                    tmp[1] = '\0';
                    this_player->leave = true;
                    MsgQueue_append(&this_lobby->queue, tmp, 2, -1);
                }
                else {
                    printf("     > Lobby %d : Player %s has quit, %d players remaning in lobby %d\n\n", this_lobby->index, this_player->name, this_lobby->players_nb, this_lobby->index);
                    // rearange players index
                    for (int i = this_player->index; i < 2; ++i) {
                        this_lobby->players[i] = this_lobby->players[i+1];
                        this_player->index++;
                    }
                    this_lobby->players_nb--;
                    for (int i = this_lobby->players_nb; i < 4; ++i) {
                        this_lobby->players[i] = NULL;
                    }
                    tmp[0] = (char)WaitingPlayers;
                    tmp[1] = this_lobby->players_nb+'0';
                    tmp[2] = '\0';
                    Lobby_broadcast(this_lobby, tmp, 3);
                    Lobby_waitAcks(this_lobby);
                }
                pthread_cond_signal(&this_lobby->send_next);
            }
            else if (this_lobby->game_ended) {
                this_lobby->players_nb--;
                if (this_lobby->players_nb == 0) {
                    // create Quit message
                    tmp[0] = (char)QuitLobby;
                    tmp[1] = '\0';
                    this_player->leave = true;
                    MsgQueue_append(&this_lobby->queue, tmp, 2, -1);
                }
                pthread_cond_signal(&this_lobby->send_next);
            }
            
            this_lobby->players[this_player->index] = NULL;
            Lobby_unlock(this_lobby, this_player);
            break;
        }
        else if (strcmp(buffer, "ack") == 0){
            this_player->client.ack = (strcmp(buffer, "ack") == 0);
        }
        else {
            int pl;
            int it;
            Lobby_lock(this_lobby, this_player);
            switch (buffer[0]) {
                case AskPlayer:
                    pl = (int)(buffer[2]-'0');
                    it = (int)(buffer[3]-'0');
                    printf("     > Lobby %d : %s is asking %s how many \"%ss\" he has. Answer : %d\n\n",
                            this_lobby->index, this_player->name, this_lobby->players[pl]->name, DATA.items_names[it], this_lobby->players[pl]->items_count[it]);
                    tmp[0] = (char)AnswerPlayer;
                    tmp[1] = pl+'0';
                    tmp[2] = it+'0';
                    tmp[3] = this_lobby->players[pl]->items_count[it]+'0';
                    tmp[4] = '\0';
                    MsgQueue_append(&this_lobby->queue, tmp, 5, -1);
                    break;

                case AskItem:
                    it = (int)(buffer[2] - '0');
                    printf("     > Lobby %d : %s is aking who has \"%s\" items\n\n", this_lobby->index, this_player->name, DATA.items_names[it]);
                    tmp[0] = (char)AnswerItem;
                    tmp[1] = it+'0';
                    for (int i = 0; i < 4; ++i) {
                        if (i == this_player->index)
                            tmp[2+i] = '?';
                        else
                            tmp[2+i] = this_lobby->players[i]->items_count[it] > 0 ? '*' : '0';
                    }
                    tmp[6] = '\0';
                    MsgQueue_append(&this_lobby->queue, tmp, 7, -1);
                    break;

                case GuessSuspect:
                    // right guess
                    if ((int)buffer[2] == this_lobby->suspect) {
                        printf("     > Lobby %d : %s guessed the suspect ! It was \"%s\" (%d)\n\n",
                                this_lobby->index, this_player->name, DATA.characters_names[(int)buffer[2]], buffer[2]);
                        tmp[0] = (char)AnswerSuspect;
                        tmp[1] = '1';
                        tmp[2] = buffer[2];
                        tmp[3] = '\0';
                        MsgQueue_append(&this_lobby->queue, tmp, 4, -1);
                        tmp[0] = (char)Victory;
                        tmp[1] = buffer[1];
                        tmp[2] = '\0';
                        MsgQueue_append(&this_lobby->queue, tmp, 3, -1);
                        this_lobby->game_ended = true;
                    }
                    // wrong guess
                    else {
                        printf("     > Lobby %d : %s made a wrong guess ! Suspect is not \"%s\" (%d)\n\n",
                                this_lobby->index, this_player->name, DATA.characters_names[(int)buffer[2]], buffer[2]);
                        this_lobby->penalities |= 1 << this_player->index;
                        tmp[0] = (char)AnswerSuspect;
                        tmp[1] = '0';
                        tmp[2] = buffer[2];
                        tmp[3] = '\0';
                        MsgQueue_append(&this_lobby->queue, tmp, 4, -1);
                    }
                    break;

                default:
                    break;
            }

            if (!this_lobby->game_ended) {
                // next turn
                this_lobby->turn = (this_lobby->turn+1)%4;
                // skip players with penalities
                while ( ((this_lobby->penalities >> this_lobby->turn)&1) == 1 ) {
                    this_lobby->penalities &= 0 << this_lobby->turn;
                    this_lobby->turn = (this_lobby->turn+1)%4;
                }
                tmp[0] = PlayerTurn;
                tmp[1] = this_lobby->turn + '0';
                tmp[2] = '\0';
                MsgQueue_append(&this_lobby->queue, tmp, 3, -1);
            }

            pthread_cond_signal(&this_lobby->send_next);
            Lobby_unlock(this_lobby, this_player);
        }
    }
    close(this_player->client.sfd);
    if (this_player != NULL) {
        deb_log("[INFO] Player %d:%s closing thread and freeing %ld bytes\n", this_lobby->index, this_player->name, sizeof(Player));
        this_lobby->players[this_player->index] = NULL;
        free(this_player);
    }
    pthread_exit(NULL);
}

int send_msg(Player* player, void* buffer, int size) {
    int r = write(player->client.sfd, buffer, size);
    char* data = (char*)buffer;
    if (r < 0) {
        deb_log("[INFO] Failed to send message to %s (lobby %d): \"%s\"\n", player->name, player->lobby->index, data);
    }
    else {
        char temp[256];
        char string[256] = "";
        sprintf(temp, "[%s:%d] %d:%s < \"%s\" = ", player->client.ip, player->client.port, player->lobby->index, player->name, data);
        strcat(string, temp);
        for (int i = 0; i < r; ++i) {
            sprintf(temp, "%02x ", data[i]);
            strcat(string, temp);
        }
        sprintf(temp, "(%d bytes)", r);
        strcat(string, temp);
        deb_log("%s\n", string);
    }
    return r;
}

int recv_msg(Player* player, void* buffer, int size) {
    int r = read(player->client.sfd, buffer, size);
    char* data = (char*)buffer;
    if (r < 0)
        deb_log("[INFO] Failed to receive message from %s [%s:%d]\n", player->name, player->client.ip, player->client.port);
    else {
        char temp[256];
        char string[256] = "";
        sprintf(temp, "[%s:%d] %d:%s > \"%s\" = ", player->client.ip, player->client.port, player->lobby->index, player->name, data);
        strcat(string, temp);
        for (int i = 0; i < r; ++i) {
            sprintf(temp, "%02x ", data[i]);
            strcat(string, temp);
        }
        sprintf(temp, "(%d bytes)", r);
        strcat(string, temp);
        deb_log("%s\n", string);
    }
    return r;
}
