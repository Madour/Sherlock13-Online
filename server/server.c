#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <netdb.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <sched.h>

#include "utils.h"
#include "typedefs.h"

#define MAX_LOBBIES 32

typedef struct GameClient {
    int sfd;
    char ip[32];
    int port;
} GameClient;

struct GameLobby;

typedef struct GamePlayer {
    GameClient client;
    char name[32];
    int index;
    bool leave;
    struct GameLobby* lobby;
} GamePlayer;

typedef struct GameLobby {
    int index;
    GamePlayer players[4];
    int players_nb;
} GameLobby;


GameLobby lobbies_array[MAX_LOBBIES];
int lobbies_states=0; // bit i set to 1 = lobby i is full, maximum of 32 lobbies

int server_sfd = -1;
struct addrinfo* server_ai;

// gère Ctrl+C
struct sigaction default_sigint;

void exit_server(int sig_no) {
    printf("\n[INFO] CTRL-C pressed (SIGINT). Closing server.\n");
    if (server_sfd >= 0) {
        close(server_sfd);
        freeaddrinfo(server_ai);
    }
    sigaction(SIGINT, &default_sigint, NULL);
    kill(getpid(), SIGINT);
}

void broadcast(GameLobby* lobby, char* msg, unsigned int size) {
    printf("[INFO] Broadcasting message to lobby %d : \"%s\"\n", lobby->index, msg);

    for (int i = 0; i < lobby->players_nb; ++i) {
        printf("     > Message sent to player %d\n", i);
        write(lobby->players[i].client.sfd, msg, sizeof(char)*size);
    }
    printf(" \n");
}

void* manage_player_thread(void* player) {
    char buffer[256];
    int msg_size;
    GamePlayer* this_player = (GamePlayer*)player;
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        if (this_player->leave) {
            printf("[INFO] Player %s leaving lobby %d\n", this_player->name, this_player->lobby->index);
            break;
        }
        msg_size = read(this_player->client.sfd, buffer, sizeof(buffer));
        if (this_player->leave) {
            printf("[INFO] Player %s leaving lobby %d\n", this_player->name, this_player->lobby->index);
            break;
        }
        if (msg_size <= 0) {
            printf("[INFO] Failed to read from client %s:%u\n", this_player->client.ip, this_player->client.port);
            printf("[INFO] Closing connection with player %s from lobby %d.\n\n", this_player->name, this_player->lobby->index);

            // tell other players in lobby that this_player quit
            if (1 < this_player->lobby->players_nb && this_player->lobby->players_nb < 4) {
                buffer[0] = WaitingPlayers;
                buffer[1] = this_player->lobby->players_nb+'0';
                buffer[2] = '\0';
                broadcast(this_player->lobby, buffer, 3);
            }
            else {
                buffer[0] = (char)QuitLobby;
                buffer[1] = '\0';
                broadcast(this_player->lobby, buffer, 2);
                for (int i = 0; i < 4; ++i) {
                    this_player->lobby->players[i].leave = true;
                }
                this_player->lobby->players_nb = 0;
                lobbies_states &= 0 << this_player->lobby->index ;
            }
            break;
        }
        printf("[%s:%d] %s > %s \n", this_player->client.ip, this_player->client.port, this_player->name, buffer);
    }

    close(this_player->client.sfd);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Commande usage : ./server <port_number> \n");
        return EXIT_SUCCESS;
    }
    if (!isUInt(argv[1])) {
        fprintf(stderr, "[Error] Argument <port_number> must be an unsigned int.\n");
        return EXIT_FAILURE;
    }

    // will call exit_server when SIGINT is received (Ctrl+C)
    struct sigaction custom_sigint;
    memset(&custom_sigint, 0, sizeof(custom_sigint));
    custom_sigint.sa_handler = &exit_server;
    sigaction(SIGINT, &custom_sigint, &default_sigint);
    
    // create socket
    server_sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sfd < 0) {
        fprintf(stderr, "[Error] Failed to create socket.\n");
        return EXIT_FAILURE;
    }

    // get server address info
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    
    getaddrinfo(NULL, argv[1], &hints, &server_ai);

    // bind server socket
    if (bind(server_sfd, server_ai->ai_addr, server_ai->ai_addrlen)) {
        fprintf(stderr, "[Error] Socket bind failed.\n");
        return EXIT_FAILURE;
    }

    // queue up to 500 connections
    listen(server_sfd, 500);
    printf(
        "[INFO] Server at %s listening on port %u\n\n",
        inet_ntoa(((struct sockaddr_in*)server_ai->ai_addr)->sin_addr),
        ntohs(((struct sockaddr_in*)server_ai->ai_addr)->sin_port)
    );
    
 
    // reset all lobbies
    memset(lobbies_array, 0, sizeof(GameLobby)*MAX_LOBBIES);
    for (int i = 0; i < MAX_LOBBIES; i++)
        lobbies_array[i].index = i;
     
    while (1) {
        // wait for new client connection and accept it
        struct sockaddr_in client_addr;
        unsigned int len = sizeof(client_addr);
        int client_sfd = accept(server_sfd, (struct sockaddr*)&client_addr, &len);
        if (client_sfd < 0) {
            fprintf(stderr, "[Error] Server accept failed.\n");
            return EXIT_FAILURE;
        }
        printf("[INFO] Accepting a connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // search for an available lobby
        int lobby_index=0;
        while( (lobbies_states>>lobby_index)&1 ) {
            if (lobby_index < MAX_LOBBIES)
                lobby_index++;
            else {
                lobby_index = -1;
                break;
            }
        }

        // no available lobbies, skip this client
        if (lobby_index == -1) {
            printf("[INFO] No available lobbies ! Closing connection with client.\n");
            close(client_sfd);
            continue;
        }

        GameLobby* lobby = &lobbies_array[lobby_index];

        if (lobby->players_nb < 4) {
            
            // get player pointer
            GamePlayer* new_player = &lobby->players[lobby->players_nb];
            
            // fill new player info
            new_player->lobby = lobby;
            new_player->index = lobby->players_nb;
            new_player->leave = false;
            new_player->client.sfd = client_sfd;
            strcpy(new_player->client.ip, inet_ntoa(client_addr.sin_addr));
            new_player->client.port = ntohs(client_addr.sin_port);
            
            // first message received from new client is player name
            char buffer[256];
            read(client_sfd, buffer, sizeof(buffer));
            strcpy(new_player->name, buffer);

            // send player index in the lobby
            buffer[0] = new_player->index+'0';
            buffer[1] = '\0';
            write(client_sfd, buffer, 2);

            printf("     > Client name : \"%s\"\n     > Joined lobby number %d\n\n", new_player->name, lobby_index);

            // create thread for the newly connected player
            pthread_t thread;
            pthread_create(&thread, NULL, manage_player_thread, new_player);
            printf("[INFO] Started thread for player \"%s\" (%ld) \n\n", new_player->name, thread);


            lobby->players_nb++;

            printf("[INFO] Number of players connected to lobby %d : %d\n\n", lobby_index, lobby->players_nb);

            if (lobby->players_nb < 4) {
                // broadcast the number of connected players to all players in lobby
                buffer[0] = (char)WaitingPlayers;
                buffer[1] = (char)lobby->players_nb+'0';
                buffer[2] = '\0';
                broadcast(lobby, buffer, 3);
            }
            else if (lobby->players_nb == 4) {
                lobbies_states |= 1 << lobby_index;
                
                buffer[0] = (char)GameStart;
                int current_i = 1;
                for (int i = 0; i < 4; ++i) {
                    int name_len = strlen(lobby->players[i].name);
                    buffer[current_i] = name_len+'0';
                    strcpy(&buffer[current_i+1], lobby->players[i].name);
                    current_i += name_len+1;
                }
                buffer[current_i] = '\0';
                broadcast(lobby, buffer, current_i+1);
                // send GameStart message
                
                printf("Lobby %d Game started !\n", lobby_index);
            }
        }
    }

    freeaddrinfo(server_ai);
    close(server_sfd);
}
