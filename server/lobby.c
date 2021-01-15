/**
 * @file lobby.c
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>

#include "common/data.h"
#include "server/lobby.h"

void Lobby_reset(Lobby* lobby) {
    for (int p = 0; p < 4; ++p) {
        lobby->players[p] = NULL;
    }

    lobby->suspect = 0;
    lobby->turn = 0;
    lobby->penalities = 0;
    lobby->state = LobbyStateWaiting;
    lobby->available = true;
    lobby->quit = false;

    MsgQueue_init(&lobby->queue);
    pthread_cond_init(&lobby->cond, NULL);

    pthread_mutex_init(&lobby->mutex, NULL);
    lobby->locked = false;
}

void Lobby_addNewPlayer(Lobby* lobby, Player* player) {
    player->lobby = lobby;
    for (int i = 0; i < 4; ++i) {
        if (lobby->players[i] == NULL) {
            lobby->players[i] = player;
            player->index = i;
            break;
        }
    }
    // reset items count
    for (int i = 0; i < 8; ++i)
        player->items_count[i] = 0;
    player->leave = false;
    lobby->players_nb++;
}

void Lobby_startGame(Lobby* lobby) {
    lobby->available = false;
    lobby->state = LobbyStateGameStarted;
    lobby->turn = 0;

    // broadcast GameStart message to lobby
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = (char)GameStart;
    int current_i = 1;
    for (int i = 0; i < 4; ++i) {
        int name_len = strlen(lobby->players[i]->name);
        buffer[current_i] = name_len+'0';
        strcpy(&buffer[current_i+1], lobby->players[i]->name);
        current_i += name_len+1;
    }
    MsgQueue_append(&lobby->queue, buffer, current_i+1, -1);

    printf("     > Lobby %d : Game started ! Distributing cards to players :\n\n", lobby->index);
    // filling players cards and adding the message to the queue
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = (char)DistribCards;
    int taken_cards = 0;
    // for each player ...
    for (int i = 0; i < 4; ++i) {
        printf("         > Player %d:%s cards : \n", lobby->index, lobby->players[i]->name);
        // .. reset item count ...
        for (int j = 0; j < 8; ++j)
            lobby->players[i]->items_count[j] = 0;
        // ... and choose 3 random cards
        for (int j = 0; j < 3; ++j) {
            int card;
            bool found = false;
            do {
                card = rand()%13;
                if ( ((taken_cards>>card)&1) == 0 ) {
                    lobby->players[i]->cards[j] = card;
                    for (int item=0; item<3; ++item) {
                        int chara_item = DATA.characters_items[card][item];
                        if (chara_item != -1)
                            lobby->players[i]->items_count[chara_item] += 1;
                    }
                    taken_cards |= 1 << card;
                    found = true;
                    printf("           - %2d = %s\n", card, DATA.characters_names[card]);
                }
            } while(!found);
            buffer[1+j] = (char)(card+1);
        }
        buffer[4] = '\0';
        MsgQueue_append(&lobby->queue, buffer, 5, i);
        printf("\n");
    }
    for (int i = 0; i < 13; ++i)
        if ( ((taken_cards >> i)&1) == 0 )
            lobby->suspect = i;

    printf("     > Lobby %d : Suspect is \"%s\" (%d)\n\n", lobby->index, DATA.characters_names[lobby->suspect], lobby->suspect);

    Lobby_printPlayers(lobby);
}

void Lobby_sendMsgs(Lobby* lobby, const char* name) {
    pthread_cond_signal(&lobby->cond);
    if (name != NULL)
        deb_log("[SIGN] Lobby %d : %s is sending signal to lobby (%lu)\n", lobby->index, name, lobby->thread);
    else
        deb_log("[SIGN] Lobby %d : Signal was sent\n", lobby->index);
}

void Lobby_lock(Lobby* lobby, const char* name) {
    if (name != NULL)
        deb_log("[LOCK] Lobby %d : %s waiting for Mutex lock\n", lobby->index, name);
    else
        deb_log("[LOCK] Lobby %d : Waiting for Mutex lock\n", lobby->index);
    // wait the lobby to be unlocked
    while(lobby->locked);
    pthread_mutex_lock(&lobby->mutex);
    lobby->locked = true;
    deb_log("[LOCK] Lobby %d is locked by %s\n", lobby->index, name == NULL ? "lobby" : name);
}

void Lobby_unlock(Lobby* lobby, const char* name) {
    deb_log("[LOCK] Lobby %d is unlocked by %s\n", lobby->index, name == NULL ? "lobby" : name);
    pthread_mutex_unlock(&lobby->mutex);
    lobby->locked = false;
}

void Lobby_broadcast(Lobby* lobby, char* msg, unsigned int size) {
    deb_log("[INFO] Lobby %d : Broadcasting message to %d players : \"%s\"\n", lobby->index, lobby->players_nb, msg);
    for (int i = 0; i < 4; ++i) {
        if (lobby->players[i] != NULL)
            if (!lobby->players[i]->leave)
                send_msg(lobby->players[i], msg, sizeof(char)*size);
    }
}

inline void Lobby_waitAcks(Lobby* lobby) {
    bool acks[4] = {false, false, false, false};
    int acks_nb = 0;
    for (int timeout = 10000; timeout > 0; --timeout) {
        for (int i = 0; i < 4; ++i) {
            if (lobby->players[i] != NULL) {
                pthread_mutex_unlock(&lobby->mutex);
                sched_yield();
                pthread_mutex_lock(&lobby->mutex);
                if (acks[i] == false && lobby->players[i]->client.ack)  {
                    deb_log("[BROAD] Lobby %d : Player %s acked\n", lobby->index, lobby->players[i]->name);
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

void Lobby_printPlayers(Lobby* lobby) {
    char res[256] = "";
    char buffer[256] = "";
    sprintf(buffer, "     > Lobby %d : Players :\n", lobby->index);
    strcat(res, buffer);
    for (int i = 0; i < 4; ++i) {
        if (lobby->players[i] != NULL) {
            sprintf(buffer, "        - %i - %s : ", i, lobby->players[i]->name);
            strcat(res, buffer);
            for (int j = 0; j < 8; ++j) {
                sprintf(buffer, "%d %s", lobby->players[i]->items_count[j], j==7?"\n":"");
                strcat(res, buffer);
            }
        }
    }
    printf("%s\n", res);
}

void* manage_lobby_thread(void* lobby) {
    Lobby* this_lobby = (Lobby*)lobby;
    MsgQueue* msg_queue = &this_lobby->queue;

    deb_log("[INFO] Lobby %d : Started thread (%ld) \n", this_lobby->index, pthread_self());
    Lobby_lock(this_lobby, NULL);
    while (1) {
        deb_log("[SIGN] Lobby %d waiting signal for sending. Lock released \n", this_lobby->index);
        
        this_lobby->locked = false;
        pthread_cond_wait(&this_lobby->cond, &this_lobby->mutex);
        this_lobby->locked = true;

        deb_log("[SIGN] Lobby %d received signal. Mutex locked \n", this_lobby->index);

        if (this_lobby->quit) {
            Lobby_unlock(this_lobby, NULL);
            break;
        }

        while (msg_queue->size > 0) {
            MsgNode* msg = MsgQueue_front(msg_queue);
            if (msg->dest == -1) {
                Lobby_broadcast(this_lobby, msg->msg, msg->size);
                // waiting acks is really optionnal, but why not
                if (msg->msg[0] != (char)QuitLobby)
                    Lobby_waitAcks(this_lobby);
            }
            else {
                Player* player = this_lobby->players[msg->dest];
                send_msg(player, msg->msg, msg->size);
            }
            if (msg->msg[0] == (char)QuitLobby && msg_queue->size == 1) {
                this_lobby->state = LobbyStateWaiting;
                this_lobby->available = true;
                this_lobby->players_nb = 0;
            }
            MsgQueue_pop(msg_queue);
        }
    
    }
    Lobby_unlock(this_lobby, NULL);

    deb_log("[INFO] Lobby %d : Closing thread (%ld) \n", this_lobby->index, pthread_self());
    this_lobby->players_nb = 0;
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
                
            Lobby_lock(this_lobby, this_player->name);
            if (this_player->leave) {
                printf("     > Lobby %d : Player %s is leaving.\n\n", this_lobby->index, this_player->name);
                Lobby_unlock(this_lobby, this_player->name);
                break;
            }
            if (this_lobby->state == LobbyStateWaiting) {
                // player was the only on in lobby
                if (this_lobby->players_nb == 1) {
                    this_lobby->players_nb--;
                    this_lobby->available = true;
                }
                else {
                    printf("     > Lobby %d : Player %s left lobby, %d players remaning in lobby %d\n\n",
                            this_lobby->index, this_player->name, this_lobby->players_nb-1, this_lobby->index);
                    if (this_lobby->players[this_player->index] == this_player) {
                        this_lobby->players_nb--;
                        this_lobby->players[this_player->index] = NULL;
                    }
                    tmp[0] = (char)WaitingPlayers;
                    tmp[1] = this_lobby->players_nb+'0';
                    tmp[2] = '\0';
                    MsgQueue_append(&this_lobby->queue, tmp, 3, -1);
                }
            }
            else if (this_lobby->state == LobbyStateGameStarted) {
                printf("     > Lobby %d : Player %s quitted game !\n\n", this_lobby->index, this_player->name);
                // create Quit message
                tmp[0] = (char)QuitLobby;
                tmp[1] = '\0';
                // send it to the other players in lobby
                for (int i = 0; i < 4; ++i) {
                    this_lobby->players[i]->leave = true;
                    if (i != this_player->index)
                        MsgQueue_append(&this_lobby->queue, tmp, 2, i);
                }
            }
            else if (this_lobby->state == LobbyStateGameEnded) {
                printf("     > Lobby %d : Player %s left lobby on ending screen.\n\n", this_lobby->index, this_player->name);
                this_lobby->players_nb--;
                if (this_lobby->players_nb == 0) {
                    this_lobby->available = true;
                    this_lobby->state = LobbyStateWaiting;
                }
            }
            else if (this_lobby->state == LobbyStateReplay) {
                // a player quit when trying to replay, cancel replay
                printf("     > Lobby %d : Player %s did not replay !\n\n", this_lobby->index, this_player->name);
                if (this_lobby->players[this_player->index] == this_player) {
                    this_lobby->players_nb--;
                    this_lobby->players[this_player->index] = NULL;
                }
                this_lobby->available = true;
                this_lobby->state = LobbyStateWaiting;
            }
            if (this_lobby->queue.size > 0)
                Lobby_sendMsgs(this_lobby, this_player->name);
            this_lobby->players[this_player->index] = NULL;
            Lobby_unlock(this_lobby, this_player->name);
            break;
        }
        else if (strcmp(buffer, "ack") == 0){
            this_player->client.ack = (strcmp(buffer, "ack") == 0);
        }
        else {
            int pl;
            int it;
            Lobby_lock(this_lobby, this_player->name);
            switch (buffer[0]) {
                case AskPlayer:
                    pl = (int)(buffer[2]-'0');
                    it = (int)(buffer[3]-'0');
                    printf("     > Lobby %d : %s is asking %s how many \"%ss\" they have. Answer : %d\n\n",
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
                        this_lobby->state = LobbyStateGameEnded;
                        
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

                case Replay:
                    printf("     > Lobby %d : %s wants to play again\n\n", this_lobby->index, this_player->name);
                    if (this_lobby->state == LobbyStateGameEnded) {
                        if (this_lobby->players_nb == 4)
                            this_lobby->state = LobbyStateReplay;
                        else {
                            this_lobby->state = LobbyStateWaiting;
                            this_lobby->available = true;
                        }
                        this_lobby->players_nb = 0;
                        for (int i = 0; i < 4; ++i)
                            this_lobby->players[i] = NULL;
                    }
                    Lobby_addNewPlayer(this_lobby, this_player);
                    tmp[0] = (char)WaitingPlayers;
                    tmp[1] = this_lobby->players_nb+'0';
                    tmp[2] = '\0';
                    MsgQueue_append(&this_lobby->queue, tmp, 3, -1);
                    if (this_lobby->players_nb == 4) {
                        Lobby_startGame(this_lobby);
                        this_lobby->turn = -1;
                    }
                    break;

                default:
                    break;
            }

            if (this_lobby->state == LobbyStateGameStarted) {
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

            Lobby_sendMsgs(this_lobby, this_player->name);
            Lobby_unlock(this_lobby, this_player->name);
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
