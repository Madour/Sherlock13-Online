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
extern bool debug;
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
    srand(time(NULL));
    debug = true;
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
        Lobby_reset(&lobbies_array[i]);
        lobbies_array[i].lobby_states = &lobbies_states;
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
        deb_log("[INFO] Accepting a connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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
        if (lobby->players_nb == 0)
            Lobby_reset(lobby);

        if (lobby->players_nb < 4) {
            
            // get player pointer
            Player* new_player = (Player*)malloc(sizeof(Player));
            lobby->players[lobby->players_nb] = new_player;

            // fill new player client info
            new_player->client.sfd = client_sfd;
            strcpy(new_player->client.ip, inet_ntoa(client_addr.sin_addr));
            new_player->client.port = ntohs(client_addr.sin_port);
            new_player->client.ack = false;
            
            // fill new player lobby info
            new_player->lobby = lobby;
            new_player->index = lobby->players_nb;
            for (int i = 0; i < 8; ++i)
                new_player->items_count[8] = 0;
            strcpy(new_player->name, "");
            new_player->leave = false;
            
            // first message received from new client is player name
            char buffer[256];
            recv_msg(new_player, buffer, sizeof(char)*32);
            strcpy(new_player->name, buffer);

            //send ack
            send_msg(new_player, "ack", 4);

            printf("     > New player name : \"%s\"\n     > Joined lobby number %d\n\n", new_player->name, lobby_index);

            // create thread for the newly connected player
            pthread_t player_thread;
            pthread_create(&player_thread, NULL, manage_player_thread, new_player);
            pthread_detach(player_thread);
            deb_log("[INFO] Started thread for player \"%s\" (%ld) \n", new_player->name, player_thread);

            lobby->players_nb++;

            deb_log("[INFO] Number of players connected to lobby %d : %d\n", lobby_index, lobby->players_nb);

            buffer[0] = (char)WaitingPlayers;
            buffer[1] = (char)lobby->players_nb+'0';
            buffer[2] = '\0';
            Lobby_lock(lobby, NULL);
            Lobby_broadcast(lobby, buffer, 3);
            Lobby_waitAcks(lobby);
            Lobby_unlock(lobby, NULL);

             if (lobby->players_nb == 4) {
                // setting lobby state to full
                lobbies_states |= 1 << lobby_index;
                // create thread for lobby and start game
                pthread_t lobby_thread;
                pthread_create(&lobby_thread, NULL, manage_lobby_thread, lobby);
                pthread_detach(lobby_thread);
            }
        }
    }

    freeaddrinfo(server_ai);
    close(server_sfd);
}
