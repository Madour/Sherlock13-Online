#include <stdlib.h>
#include <stdbool.h>

#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

#include <pthread.h>
#include <sched.h>

#include "common/utils.h"
#include "common/typedefs.h"
#include "client/SDLex.h"
#include "client/game.h"

#define WINDOW_W 1024
#define WINDOW_H 768

char* host_name;
char* port;
int socket_fd = -1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
Game game;

int send_msg(int sfd, void* buffer, int size) {
    int r = write(sfd, buffer, size);
    char* data = (char*)buffer;
    if (r < 0) {
        printf("[INFO] Failed to send message : \"%s\"\n\n", data);
    }
    else {
        char temp[256];
        char string[256] = "";
        sprintf(temp, "[%s:%s] < \"%s\" = ", host_name, port, data);
        strcat(string, temp);
        for (int i = 0; i < r; ++i) {
            sprintf(temp, "%02x ", data[i]);
            strcat(string, temp);
        }
        sprintf(temp, "(%d bytes)", r);
        strcat(string, temp);
        printf("%s\n\n", string);
    }
    return r;
}

int recv_msg(int sfd, void* buffer, int size) {
    int r = read(sfd, buffer, size);
    char* data = (char*)buffer;
    if (r < 0)
        printf("[INFO] Failed to receive message from %s:%s\n\n", host_name, port);
    else {
        char temp[256];
        char string[256] = "";
        sprintf(temp, "[%s:%s] > \"%s\" = ", host_name, port, data);
        strcat(string, temp);
        for (int i = 0; i < r; ++i) {
            sprintf(temp, "%02x ", data[i]);
            strcat(string, temp);
        }
        sprintf(temp, "(%d bytes)", r);
        strcat(string, temp);
        printf("%s\n\n", string);
    }
    return r;
}

void* receive_server_msgs_thread(void* args) {
    char buffer[256];
    while(1) {
        if (!game.connected)
            break;
        int msg_size = recv_msg(socket_fd, buffer, sizeof(buffer));

        if (msg_size <= 0) {
            game.connected = false;
            break;
        }
        int current_i = 1;
        int len;

        pthread_mutex_lock(&mutex);
        switch (buffer[0]) {

            case (int)WaitingPlayers:
                game.players_nb = buffer[1]-'0';
                sprintf(buffer, "Waiting for %d player%s...", 4-game.players_nb, game.players_nb >= 3 ? "":"s");
                SDLex_TextSetString(game.texts.wait_players, buffer);
            
                if (game.players_nb < 4)
                    printf("Waiting for %d more player%s\n\n", 4-game.players_nb, game.players_nb>=3 ? "":"s");
                break;

            case (int)GameStart:
                current_i = 1;
                printf("Receiving player names : \n");
                // fill players name infos
                for (int i = 0; i < 4; ++i) {
                    len = buffer[current_i] - '0';
                    memcpy(game.players[i].name, &buffer[current_i+1], sizeof(char)*len);
                    game.players[i].name[len] = '\0';
                    game.texts.player_names[i] = SDLex_CreateText(game.renderer, game.players[i].name, game.font);
                    current_i += len+1;
                    printf("   - %s\n", game.players[i].name);
                }
                printf("Game started !\n\n");
                break;

            case (int)DistribCards:
                printf("Received my cards : \n");
                for (int i = 0; i < 3; ++i) {
                    printf("    - %s\n", DATA.character_names[((int)buffer[1+i])-1]);
                }
                break;

            case (int)QuitLobby:
                game.connected = false;
                for (int i = 0; i < 4; ++i) {
                    if (game.texts.player_names[i] != NULL)
                        SDLex_DestroyText(game.texts.player_names[i]);
                    game.texts.player_names[i] = NULL;
                }
                break;

            default:
                printf("Command not recognized\n\n");
                break;
        }
        send_msg(socket_fd, "ack", 4);

        pthread_mutex_unlock(&mutex);
    }
    close(socket_fd);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Commande usage : ./game <host_name> <port_number> <player_name>\n");
        return EXIT_SUCCESS;
    }
    if (!isUInt(argv[2])) {
        fprintf(stderr, "[ERROR] Argument <port_number> must be an unsigned int.\n");
        return EXIT_FAILURE; 
    }

    host_name = argv[1];
    port = argv[2];
    char* player_name = argv[3];
    int len = strlen(player_name);
    player_name[(len > 32 ? 32 : len)] = '\0';

    printf("Player %s\n", player_name);

    struct addrinfo* server_ai;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    getaddrinfo(host_name, port, &hints, &server_ai);
    
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
    TTF_Init();

    SDL_Renderer* renderer;
    SDL_Window* window;

    SDL_CreateWindowAndRenderer(WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL, &window, &renderer);
    char window_title[256] = "Sherlock 13 Online !";
    SDL_SetWindowTitle(window, window_title);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (window == NULL || renderer == NULL) {
        fprintf(stderr, "Failed to create SDL_Window or SDL_Renderer : %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    Game_init(&game, renderer);

    while(!game.quit) {
        Uint64 start_time = SDL_GetPerformanceCounter();

        game.mouse_click = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                game.quit = true;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                SDL_GetMouseState(&game.mouse_pos.x, &game.mouse_pos.y);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                game.mouse_click = true;
            }
        }

        pthread_mutex_lock(&mutex);
        
        Game_update(&game);

        if (game.mouse_click) {
            // connect to server when button pressed : 
            if (!game.connected) {
                SDL_Rect cell = SDLex_GridGetCellRect(&game.grid1, -1, 0);
                if (SDL_PointInRect(&game.mouse_pos, &cell)) {
                    // create socket and connect to server
                    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (socket_fd < 0) {
                        fprintf(stderr, "[ERROR] Failed to create socket\n");
                        return EXIT_FAILURE;
                    }
                    if (connect(socket_fd, server_ai->ai_addr, server_ai->ai_addrlen)) {
                        fprintf(stderr, "[ERROR] Connection with the server failed.\n");
                        perror("connect");
                        return EXIT_FAILURE;
                    }
                    game.connected = true;
                    printf("[INFO] Connected to server %s:%u \n\n", inet_ntoa(((struct sockaddr_in*)server_ai->ai_addr)->sin_addr), ntohs(((struct sockaddr_in*)server_ai->ai_addr)->sin_port));
                    
                    // send player name
                    int msg_size = send_msg(socket_fd, player_name, strlen(player_name)+1);
                    if (msg_size <= 0) {
                        fprintf(stderr, "[ERROR] Failed to send message to server. Closing connection.\n\n");
                        close(socket_fd);
                        game.connected = false;
                    }
                    //wait serevr ack
                    char buffer[4];
                    recv_msg(socket_fd, buffer, 4);

                    // start thread asap
                    pthread_t thread;
                    pthread_create(&thread, NULL, receive_server_msgs_thread, NULL);
                    printf("[INFO] Waiting for server messages in thread %lu \n\n", thread);
                }
            }
            /*else {
                // send mouse coordinates (for testing)
                char buffer[256];
                sprintf(buffer, "(%d, %d)", game.mouse_pos.x, game.mouse_pos.y);
                int msg_size = send_msg(socket_fd, buffer, strlen(buffer));
                if (msg_size <= 0) {
                    printf("[ERROR] Failed to write to server. Closing connection.\n\n");
                    close(socket_fd);
                    game.connected = false;
                }
            }*/
        }

        Game_render(&game);
        pthread_mutex_unlock(&mutex);
        
        // limit FPS
        float elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        SDL_Delay(fmax(0, 33.33 - elapsed_s*1000));
        elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        sprintf(window_title, "Sherlock 13 Online ! %s - FPS : %f - frame duration : %f ms", player_name, 1.0/elapsed_s, elapsed_s*1000);
        SDL_SetWindowTitle(window, window_title);
    }

    Game_terminate(&game);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    freeaddrinfo(server_ai);
    close(socket_fd);

    return EXIT_SUCCESS;
}
