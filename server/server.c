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

#include "common/utils.h"
#include "common/typedefs.h"
#include "server/lobby.h"
#include "server/msg_queue.h"


Lobby lobbies_array[MAX_LOBBIES];
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


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Commande usage : ./server <port_number> \n");
        return EXIT_SUCCESS;
    }
    if (!isUInt(argv[1])) {
        fprintf(stderr, "[ERROR] Argument <port_number> must be an unsigned int.\n");
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
        fprintf(stderr, "[ERROR] Failed to create socket.\n");
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
        fprintf(stderr, "[ERROR] Socket bind failed.\n");
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
    memset(lobbies_array, 0, sizeof(Lobby)*MAX_LOBBIES);
    for (int i = 0; i < MAX_LOBBIES; i++) {
        lobbies_array[i].index = i;
        lobbies_array[i].send_next = false;
        lobbies_array[i].lobby_states = &lobbies_states;
        MsgQueue_Init(&lobbies_array[i].queue);
        for (int p = 0; p < 4; ++p)
            lobbies_array[i].players[p] = NULL;
    }
    
    while (1) {
        // wait for new client connection and accept it
        struct sockaddr_in client_addr;
        unsigned int len = sizeof(client_addr);
        int client_sfd = accept(server_sfd, (struct sockaddr*)&client_addr, &len);
        if (client_sfd < 0) {
            fprintf(stderr, "[ERROR] Server accept failed.\n");
            return EXIT_FAILURE;
        }
        printf("[INFO] Accepting a connection from %s:%d\n\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // search for an available lobby
        int lobby_index=0;
        while( (lobbies_states>>lobby_index)&1 ) {
            if (lobby_index < MAX_LOBBIES-1)
                lobby_index++;
            else {
                lobby_index = -1;
                break;
            }
        }

        // no available lobbies, decline the client
        if (lobby_index == -1) {
            printf("[INFO] No available lobbies ! Closing connection with client.\n");
            close(client_sfd);
            continue;
        }

        Lobby* lobby = &lobbies_array[lobby_index];

        if (lobby->players_nb < 4) {
            
            // get player pointer
            Player* new_player = malloc(sizeof(Player));
            lobby->players[lobby->players_nb] = new_player;

            // fill new player client info
            new_player->client.sfd = client_sfd;
            strcpy(new_player->client.ip, inet_ntoa(client_addr.sin_addr));
            new_player->client.port = ntohs(client_addr.sin_port);
            new_player->client.ack = false;
            // fill new player lobby info
            new_player->lobby = lobby;
            new_player->index = lobby->players_nb;
            new_player->leave = false;
            
            
            // first message received from new client is player name
            char buffer[256];
            recv_msg(new_player, buffer, sizeof(buffer));
            strcpy(new_player->name, buffer);

            // read ack
            printf("[INFO] Waiting player ack\n");
            recv_msg(new_player, buffer, 4);

            printf("     > New player name : \"%s\"\n     > Joined lobby number %d\n\n", new_player->name, lobby_index);

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
                Lobby_Broadcast(lobby, buffer, 3);
                Lobby_WaitAcks(lobby);
            }
            else if (lobby->players_nb == 4) {
                // setting lobby state to full
                lobbies_states |= 1 << lobby_index;
                // create thread for lobby and start game
                pthread_create(&thread, NULL, manage_lobby_thread, lobby);
            }
        }
    }

    freeaddrinfo(server_ai);
    close(server_sfd);
}
