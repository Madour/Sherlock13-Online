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
Game game;

void* receive_server_msgs_thread(void* args) {
    char buffer[256];
    while(1) {
        if (!game.connected)
            break;
        int msg_size = read(socket_fd, buffer, sizeof(buffer));
        if (msg_size <= 0) {
            game.connected = false;
            break;
        }
        printf("[%s:%s] > \"%s\"\n\n", host_name, port, buffer);
        int current_i = 1;
        int len;
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
                SDLex_DestroyText(game.texts.player_names[game.my_index]);
                // fill players name infos
                for (int i = 0; i < 4; ++i) {
                    len = buffer[current_i] - '0';
                    memcpy(game.players[i].name, &buffer[current_i+1], sizeof(char)*len);
                    game.players[i].name[len] = '\0';
                    game.texts.player_names[i] = SDLex_CreateText(game.renderer, game.players[i].name, game.font);
                    current_i += len+1;
                }
                printf("Game started !\n\n");
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
    if (strlen(player_name) > 32)
        player_name[32] = '\0';

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

    char* characters_names[13] = {
        "Sebastian Moran",
        "Irene Adler",
        "Inspector Lestrade",
        "Inspector Gregson",
        "Inspector Baynes",
        "Inspector Bradstreet",
        "Inspector Hopkins",
        "Sherlock Holmes",
        "John Watson",
        "Mycroft Holmes",
        "Mrs. Hudson",
        "Mary Morstan",
        "James Moriarty"
    };
    // characters_items[character_my_] contains {item1_index, item2_index, item3_index}
    int characters_items[13][3] = {
        {4, 7, -1},     // seb moran
        {3, 4, 2},      // iren adler
        {3, 5, 1},      // insp lestrade
        {3, 7, 1},      //insp gregson
        {3, 0, -1},     // insp baynes
        {3, 7, -1},     // insp bradstreet
        {3, 6, 5},      // insp hopkins
        {6, 0, 7},      // sherlock
        {6, 5, 7},      // watson
        {6, 0, 1},      // mycroft
        {6, 2, -1},     // hudson
        {1, 2, -1},     // mary morstan
        {4, 0, -1},     // james moriarty
    };

    game.data.character_names = characters_names;
    game.data.character_items = &characters_items[0][0];
    Game_Init(&game, renderer);

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
        
        Game_Update(&game);

        if (game.mouse_click) {
            // connect to server when button pressed : 
            if (!game.connected) {
                SDL_Rect cell = SDLex_GridGetCellRect(&game.grid1, -1, 0);
                if (SDL_PointInRect(&game.mouse_pos, &cell)) {
                    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (socket_fd < 0) {
                        fprintf(stderr, "[ERROR] Failed to create socket\n");
                        return EXIT_FAILURE;
                    }
                    if (connect(socket_fd, server_ai->ai_addr, server_ai->ai_addrlen)) {
                        fprintf(stderr, "[ERROR] Connection with the server failed.\n");
                        perror("connect");
                        return 1;
                    }
                    game.connected = true;
                    printf("[INFO] Connected to server %s:%u \n", inet_ntoa(((struct sockaddr_in*)server_ai->ai_addr)->sin_addr), ntohs(((struct sockaddr_in*)server_ai->ai_addr)->sin_port));
                    int msg_size = write(socket_fd, player_name, sizeof(char)*32);
                    if (msg_size <= 0) {
                        printf("[ERROR] Failed to send message to server. Closing connection.\n\n");
                        close(socket_fd);
                        game.connected = false;
                    }
                    printf(" > \"%s\"\n", player_name);

                    // reading server message : game.my_index
                    char buffer[64];
                    msg_size = read(socket_fd, buffer, sizeof(buffer));
                    printf("[%s:%s] > \"%s\"\n", host_name, port, buffer);
                    game.my_index = buffer[0] - '0';
                    strcpy(game.players[game.my_index].name, player_name);
                    
                    // start thread asap
                    pthread_t thread;
                    pthread_create(&thread, NULL, receive_server_msgs_thread, NULL);
                    printf("[INFO] Start waiting for server messages in thread %lu \n", thread);

                    // fill the rest of game data
                    game.players[game.my_index].index = game.my_index;
                    game.players_nb = game.my_index+1;
                    game.texts.player_names[game.my_index] = SDLex_CreateText(renderer, player_name, game.font);
                }
            }
            else {
                // send mouse coordinates (for testing)
                char buffer[256];
                sprintf(buffer, "(%d, %d)", game.mouse_pos.x, game.mouse_pos.y);
                int msg_size = write(socket_fd, buffer, strlen(buffer));
                if (msg_size <= 0) {
                    printf("[ERROR] Failed to write to server. Closing connection.\n\n");
                    close(socket_fd);
                    game.connected = false;
                }
                else {
                    printf(" > \"%s\"\n", buffer);
                }
            }
        }

        Game_Render(&game);
        
        // limit FPS
        float elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        SDL_Delay(fmax(0, 16.66 - elapsed_s*1000));
        elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        sprintf(window_title, "Sherlock 13 Online ! - FPS : %f - frame duration : %f ms", 1.0/elapsed_s, elapsed_s*1000);
        SDL_SetWindowTitle(window, window_title);
    }

    Game_Terminate(&game);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    freeaddrinfo(server_ai);
    close(socket_fd);

    return EXIT_SUCCESS;
}
