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
char* my_name;
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
        int name_len;

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
                game.started = true;
                printf("Received player names : \n");
                // fill players name infos
                for (int i = 0; i < 4; ++i) {
                    // get name length
                    name_len = buffer[current_i] - '0';
                    // copy name string to player name field
                    memcpy(game.players[i].name, &buffer[current_i+1], sizeof(char)*name_len);
                    game.players[i].name[name_len] = '\0';
                    // if name is my name, fill the shortcuts
                    if (strcmp(game.players[i].name, my_name) == 0) {
                        game.me = &game.players[i];
                        game.my_index = i;
                    }
                    game.texts.players_names[i] = SDLex_CreateText(game.renderer, game.players[i].name, game.font);
                    SDL_Point pos = {game.grid1.position.x+20 , game.grid1.position.y+15+(game.grid1.cell_size.y)*(i+1)};
                    SDLex_TextSetPosition(game.texts.players_names[i], pos.x, pos.y);
                    current_i += name_len+1;
                    printf("   - %s\n", game.players[i].name);
                }
                char tmp[64]; sprintf(tmp, "It is %s's turn.", game.players[game.turn].name);
                SDLex_TextSetString(game.texts.who_is_playing, tmp);
                    
                printf("Game started !\n\n");
                break;

            case (int)DistribCards:
                printf("Received my cards : \n");
                memset(game.players_items_count[game.my_index], 0, sizeof(int)*8);
                for (int c = 0; c < 3; ++c) {
                    int card_id = ((int)buffer[1+c])-1;
                    game.me->cards[c] = card_id;
                    game.selected.checkmarks[card_id] = 1;

                    // get character items and fill player info
                    for (int i = 0; i < 3; ++i) {
                        int chara_item = game.data->character_items[card_id][i];
                        if (chara_item != -1) {
                            if (game.players_items_count[game.my_index][chara_item] < 0)
                                game.players_items_count[game.my_index][chara_item] = 1;
                            else
                                game.players_items_count[game.my_index][chara_item]+= 1;
                        }
                    }

                    game.sprites.cards[c].texture = game.textures.cards[card_id];
                    printf("    - %s\n", DATA.character_names[card_id]);
                }
                printf("\n");
                break;

            case (int)QuitLobby:
                Game_reset(&game);
                for (int i = 0; i < 4; ++i) {
                    if (game.texts.players_names[i] != NULL)
                        SDLex_DestroyText(game.texts.players_names[i]);
                    game.texts.players_names[i] = NULL;
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
    my_name = argv[3];
    int len = strlen(my_name);
    my_name[(len > 32 ? 32 : len)] = '\0';

    printf("Player %s\n", my_name);

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
                        fprintf(stderr, "[ERROR] Connection to the server failed.\n");
                        perror("connect");
                        return EXIT_FAILURE;
                    }
                    game.connected = true;
                    printf("[INFO] Connected to server %s:%u \n\n", inet_ntoa(((struct sockaddr_in*)server_ai->ai_addr)->sin_addr), ntohs(((struct sockaddr_in*)server_ai->ai_addr)->sin_port));
                    
                    // send player name
                    int msg_size = send_msg(socket_fd, my_name, strlen(my_name)+1);
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
        }

        Game_render(&game);
        pthread_mutex_unlock(&mutex);
        
        // limit FPS
        float elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        SDL_Delay(fmax(0, 33.33 - elapsed_s*1000));
        elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        sprintf(window_title, "Sherlock 13 Online ! %s - FPS : %f - frame duration : %f ms", my_name, 1.0/elapsed_s, elapsed_s*1000);
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
