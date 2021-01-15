/**
 * @file server.c
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

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
#include "common/data.h"
#include "server/lobby.h"
#include "server/msg_queue.h"


Lobby lobbies_array[MAX_LOBBIES];

extern bool debug;
int server_sfd = -1;
struct addrinfo* server_ai;


struct sigaction default_sigint;

// server will most likely be stopped by Ctrl+C (SIGINT)
void exit_server(int sig_no) {
    printf("\n[INFO] CTRL-C pressed (SIGINT). Closing server.\n");
    // tell lobby threads to quit and wait them to join the main thread
    printf("[INFO] Server : Waiting lobbies threads to terminate and join\n\n");
    for (int i = 0; i < MAX_LOBBIES; ++i) {
        lobbies_array[i].quit = true;
        Lobby_lock(&lobbies_array[i], "Server");
        Lobby_sendMsgs(&lobbies_array[i], "Server");
        Lobby_unlock(&lobbies_array[i], "Server");
        pthread_join(lobbies_array[i].thread, NULL);
        printf("\r[INFO] Thread %i/%ld deleted.%s", i+1, MAX_LOBBIES, debug ? "\n\n" : "");
        fflush(stdout);
    }
    printf("\n\n");
    if (server_sfd >= 0) {
        close(server_sfd);
        freeaddrinfo(server_ai);
    }
    sigaction(SIGINT, &default_sigint, NULL);
    kill(getpid(), SIGINT);
}


int main(int argc, char* argv[]) {
    srand(time(NULL));
    debug = false;

    if (argc < 2) {
        printf("Commande usage : ./server <port_number> \n");
        return EXIT_SUCCESS;
    }
    if (!isUInt(argv[1])) {
        fprintf(stderr, "[ERROR] Argument <port_number> must be an unsigned int.\n");
        return EXIT_FAILURE;
    }
    if (argc > 2) {
        if (strcmp(argv[2], "-d") == 0 || strcmp(argv[2], "--debug") == 0) {
            debug = true;
        }
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
    printf("[INFO] Server : Creating thread pool. \n\n");
    for (int i = 0; i < MAX_LOBBIES; i++) {
        lobbies_array[i].index = i;
        Lobby_reset(&lobbies_array[i]);
        pthread_t lobby_thread;
        pthread_create(&lobby_thread, NULL, lobby_thread_func, &lobbies_array[i]);
        lobbies_array[i].thread = lobby_thread;
        printf("\r[INFO] Thread %i/%ld created.%s", i+1, MAX_LOBBIES, debug ? "\n\n" : "");
        fflush(stdout);
    }
    printf("%s[INFO] Server : Thread pool created. \n\n", debug ? "":"\n\n");

    // main server loop
    while (1) {
        // wait for new client connection and accept it
        struct sockaddr_in client_addr;
        unsigned int len = sizeof(client_addr);
        int client_sfd = accept(server_sfd, (struct sockaddr*)&client_addr, &len);
        if (client_sfd < 0) {
            fprintf(stderr, "[ERROR] Server accept failed.\n");
            return EXIT_FAILURE;
        }
        deb_log("[INFO] Server : Accepting a connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // search for an available lobby, find the one with the most players waiting in
        int lobby_index = -1;
        int max_players_nb = -1;
        deb_log("[INFO] Server : Searching for an available lobby");
        for (int i = 0; i < MAX_LOBBIES; ++i) {
            pthread_mutex_lock(&lobbies_array[i].mutex);
            if (lobbies_array[i].available) {
                if (lobbies_array[i].players_nb > max_players_nb) {
                    deb_log("       Found Lobby %d with %d players\n", i, lobbies_array[i].players_nb);
                    lobby_index = i;
                    max_players_nb = lobbies_array[i].players_nb;
                }
            }
            pthread_mutex_unlock(&lobbies_array[i].mutex);
        }

        // no available lobbies, decline the client
        if (lobby_index == -1) {
            deb_log("[INFO] Server : No available lobbies ! Closing connection with client.\n");
            close(client_sfd);
            continue;
        }

        Lobby* lobby = &lobbies_array[lobby_index];

        // security check, but should always be true
        if (lobby->players_nb < 4) {
            
            // create a new player
            Player* new_player = (Player*)malloc(sizeof(Player));

            // fill new player client information
            new_player->client.sfd = client_sfd;
            strcpy(new_player->client.ip, inet_ntoa(client_addr.sin_addr));
            new_player->client.port = ntohs(client_addr.sin_port);
            new_player->client.ack = false;
            
            strcpy(new_player->name, "???");
            
            // add new player to lobby
            Lobby_lock(lobby, "Server");
            Lobby_addNewPlayer(lobby, new_player);
            Lobby_unlock(lobby, "Server");

            // first message received from new client is player name
            char buffer[256];
            recv_msg(new_player, buffer, sizeof(char)*32);
            strcpy(new_player->name, buffer);
            //send ack
            send_msg(new_player, "ack", 4);

            
            printf("     > New player name : \"%s\"\n     > Joined lobby number %d\n\n", new_player->name, lobby_index);

            // create thread for the newly connected player
            pthread_t player_thread;
            pthread_create(&player_thread, NULL, player_thread_func, new_player);
            pthread_detach(player_thread);
            deb_log("[INFO] Started thread for player \"%s\" (%ld) \n", new_player->name, player_thread);

            deb_log("[INFO] Server : Number of players connected to lobby %d : %d\n", lobby_index, lobby->players_nb);

            // broadcast to the lobby to tell other players a new player just connected
            buffer[0] = (char)WaitingPlayers;
            buffer[1] = (char)lobby->players_nb+'0';
            buffer[2] = '\0';
            Lobby_lock(lobby, "Server");
            MsgQueue_append(&lobby->queue, buffer, 3, -1);
            // if this player is the last one, start the game
            if (lobby->players_nb == 4)
                Lobby_startGame(lobby);
            Lobby_sendMsgs(lobby, "Server");
            Lobby_unlock(lobby, "Server");
        }
    }

    freeaddrinfo(server_ai);
    close(server_sfd);
}
