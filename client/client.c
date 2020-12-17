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

#include "SDLex.h"
#include "utils.h"

#define WINDOW_W 1024
#define WINDOW_H 768

int socket_fd = -1;
volatile bool connected = false;
volatile bool game_started = false;

int my_index;
typedef struct Player {
    char name[32];
    char index;
} Player;

int connected_players_nb = 0;

enum Actions {
    WaitingPlayers,
    StartGame
};

void* receive_server_msgs_thread(void* args) {
    char buffer[256];
    while(1) {
        if (!connected)
            break;
        int msg_size = read(socket_fd, buffer, sizeof(buffer));
        if (msg_size <= 0) {
            connected = false;
            break;
        }
        printf("[INFO] Read from server : %s\n", buffer);
        switch (buffer[0]) {
            case WaitingPlayers:
                connected_players_nb = (int)buffer[1];
                if (connected_players_nb < 4)
                    printf("Waiting for %d more player%s", 4-connected_players_nb, connected_players_nb==3 ? "":"s");
                break;

            case StartGame:
                break;

            default:
                break;
        }
    }
    pthread_exit(pthread_self);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Commande usage : ./game <host_name> <port_number> <player_name>\n");
        return EXIT_SUCCESS;
    }
    if (!isUInt(argv[2])) {
        fprintf(stderr, "Error : Argument <port_number> must be an unsigned int.\n");
        return EXIT_FAILURE;
    }

    char* host_name = argv[1];
    char* port = argv[2];
    char* player_name = argv[3];
    if (strlen(player_name) > 32)
        player_name[32] = '\0';

    printf("Player %s\n", player_name);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        fprintf(stderr, "Error : failed to create socket\n");
        return EXIT_FAILURE;
    }

    struct addrinfo* server_ai;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    getaddrinfo(host_name, port, &hints, &server_ai);

    char* characters_names[] = {
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
        {4, 7, -1}, // seb moran
        {3, 4, 2}, // iren adler
        {3, 5, 1}, // insp lestrade
        {3, 7, 1}, //insp gregson
        {3, 0, -1}, // insp baynes
        {3, 7, -1}, // insp bradstreet
        {3, 6, 5}, // insp hopkins
        {6, 0, 7}, // sherlock
        {6, 5, 7}, // watson
        {6, 0, 1}, // mycroft
        {6, 2, -1}, // hudson
        {1, 2, -1}, // mary morstan
        {4, 0, -1}, // james moriarty
    };
    // selected cells in the grids
    int selected_item = -1;
    int selected_player = -1;
    int selected_character = -1;
    int checked_characters[13] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

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

    // open the font file
    TTF_Font* font_15 = TTF_OpenFont("assets/sans.ttf", 15);

    char buffer[255];

    // load all cards textures
    SDL_Texture* cards_textures[13];
    for (int i = 0; i < 13; ++i) {
        sprintf(buffer, "assets/card_%d.png", i);
        cards_textures[i] = SDLex_LoadTextureFromFile(renderer, buffer);
    }
    // load all items icons textures
    SDL_Texture* items_textures[13];
    for (int i = 0; i < 8; ++i) {
        sprintf(buffer, "assets/item_%d.png", i);
        items_textures[i] = SDLex_LoadTextureFromFile(renderer, buffer);
    }
    // load buttons textures
    SDL_Texture* connectbtn_texture = SDLex_LoadTextureFromFile(renderer, "assets/connectbutton.png");
    SDL_Texture* gobtn_texture = SDLex_LoadTextureFromFile(renderer, "assets/gobutton.png");

    // create the grids
    SDLex_Grid grid1;
    grid1.position = (SDL_Point){10, 10};
    grid1.cell_size = (SDL_Point){60, 50};
    grid1.padding_topleft = (SDL_Point){240, 0};
    grid1.padding_bottomright = (SDL_Point){0, 0};
    grid1.colomns_nb = 8; grid1.rows_nb = 5;

    SDLex_Grid grid2;
    grid2.position = (SDL_Point){10, 270};
    grid2.cell_size = (SDL_Point){200, 37};
    grid2.padding_topleft = (SDL_Point){37*3, 0};
    grid2.padding_bottomright = (SDL_Point){40, 0};
    grid2.colomns_nb = 1; grid2.rows_nb = 13;

    // create buttons sprites
    SDLex_Sprite connect_btn = {connectbtn_texture, {}, {grid1.position.x+30, grid1.position.y+2}, {0.9, 0.9}};
    SDLex_Sprite go_btn = {gobtn_texture, {}, {1, 1}, {1.0, 1.0}};

    // create player cards sprites
    SDLex_Sprite card0_spr = {cards_textures[3], {}, {750, 110}, {0.25, 0.25}};
    SDLex_Sprite card2_spr = {cards_textures[1], {}, {750, 310}, {0.25, 0.25}};
    SDLex_Sprite card1_spr = {cards_textures[5], {}, {750, 510}, {0.25, 0.25}};

    // create sprites and a text for each item
    SDLex_Text* items_texts[8];
    SDLex_Sprite items_sprites[8];
    for (int i = 0; i < 8; ++i) {
        items_sprites[i].texture = items_textures[i];
        items_sprites[i].texture_rect = (SDL_Rect){0, 0, 0, 0};
        items_sprites[i].position = (SDL_Point){grid1.position.x+grid1.padding_topleft.x+grid1.cell_size.x*i+5, grid1.position.y};
        items_sprites[i].scale = (SDL_FPoint){0.3, 0.3};
        if (i < 4)
            items_texts[i] = SDLex_CreateText(renderer, "8", font_15);
        else if (i > 4)
            items_texts[i] = SDLex_CreateText(renderer, "3", font_15);
        else
            items_texts[i] = SDLex_CreateText(renderer, "4", font_15);
        SDLex_TextSetPosition(items_texts[i], grid1.position.x+grid1.padding_topleft.x+grid1.cell_size.x*(i+1)-11, grid1.position.y+grid1.cell_size.y-20);
    }

    // characters names texts
    SDLex_Text* characters_texts[13];
    for (int i = 0; i < 13; ++i) {
        characters_texts[i] = SDLex_CreateText(renderer, characters_names[i], font_15);
        SDLex_TextSetPosition(characters_texts[i], 205, grid2.position.y +25*i);
    }

    SDL_Point mouse_pos;
    
    bool quit = false;
    while(!quit) {
        Uint64 start_time = SDL_GetPerformanceCounter();
        bool mouse_click = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                SDL_GetMouseState(&mouse_pos.x, &mouse_pos.y);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                mouse_click = true;
            }
        }

        // START UPDATE
        SDL_Rect cell;
        int hovered_item = -1;
        int hovered_player = -1;
        int hovered_character = -1;
        int hovered_checkmark = -1;

        // check if hovering/selecting an item cell
        for (int i = 0; i < grid1.colomns_nb; ++i) {
            cell = SDLex_GridGetCellRect(&grid1, i, 0);
            if (SDL_PointInRect(&mouse_pos, &cell)) {
                hovered_item = i;
                if (mouse_click)
                    selected_item = selected_item == i ? -1 : i;
                break;
            }
        }
        // check if hovering/selecting a player name cell
        for (int i = 1; i < grid1.rows_nb; ++i) {
            cell = SDLex_GridGetCellRect(&grid1, -1, i);
            if (SDL_PointInRect(&mouse_pos, &cell)) {
                hovered_player = i-1;
                if (mouse_click)
                    selected_player = selected_player == i-1 ? -1 : i-1;
                break;
            }
        }
        if (selected_item > -1 || selected_player > -1)
            selected_character = -1;
        // check if hovering/selecting a character name cell
        for (int i = 0; i < grid2.rows_nb; ++i) {
            cell = SDLex_GridGetCellRect(&grid2, 0, i);
            if (SDL_PointInRect(&mouse_pos, &cell)) {
                hovered_character = i;
                if (mouse_click)
                    selected_character = selected_character == i ? -1 : i;
                break;
            }
        }
        if (selected_character > -1)
            selected_item = selected_player = -1;
        // check if hovering/selecting check mark cell
        for (int i = 0; i < grid2.rows_nb; ++i) {
            cell = SDLex_GridGetCellRect(&grid2, 1, i);
            if (SDL_PointInRect(&mouse_pos, &cell)) {
                hovered_checkmark = i;
                if (mouse_click) {
                    checked_characters[i] *= -1;
                }
                break;
            }
        }

        // connect to server when button pressed : 
        if (mouse_click) {
            if (!connected) {
                cell = SDLex_GridGetCellRect(&grid1, -1, 0);
                if (SDL_PointInRect(&mouse_pos, &cell)) {
                    if (connect(socket_fd, server_ai->ai_addr, server_ai->ai_addrlen)) {
                        fprintf(stderr, "Error : connection with the server failed.\n");
                        perror("connect");
                        return 1;
                    }
                    connected = true;
                    printf("[INFO] Connected to server %s:%u \n", inet_ntoa(((struct sockaddr_in*)server_ai->ai_addr)->sin_addr), ntohs(((struct sockaddr_in*)server_ai->ai_addr)->sin_port));
                    int msg_size = write(socket_fd, player_name, sizeof(char)*32);
                    printf("[INFO] Sent message of size %d : %s\n", msg_size, player_name);

                    // reading server message : number of players in lobby and their names
                    char buffer[32];
                    msg_size = read(socket_fd, buffer, sizeof(buffer));
                    printf("[INFO] Received server response : %s\n", buffer);

                    pthread_t thread;
                    pthread_create(&thread, NULL, receive_server_msgs_thread, NULL);
                    printf("[INFO] Start waiting for server messages in thread %lu \n", thread);

                }
            }
            else {
                char buffer[256];
                sprintf(buffer, "(%d, %d)", mouse_pos.x, mouse_pos.y);
                int msg_size = write(socket_fd, buffer, strlen(buffer));
                if (msg_size <= 0) {
                    printf("[INFO] Failed to write to server\n");
                    printf("[INFO] Closing connection.\n\n");
                    close(socket_fd);
                    connected = false;
                }
                else {
                    printf("[INFO] Sent message of size %d : %s\n", msg_size, buffer);
                }
            }
        }
        // END UPDATE

        // START RENDERING
        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        // draw cards
        SDLex_RenderDrawSprite(renderer, &card0_spr);
        SDLex_RenderDrawSprite(renderer, &card1_spr);
        SDLex_RenderDrawSprite(renderer, &card2_spr);

        // fill hovered item cell
        if (hovered_item != -1) {
            cell = SDLex_GridGetCellRect(&grid1, hovered_item, 0);
            SDL_SetRenderDrawColor(renderer, 20, 240, 80, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered player name cell
        if (hovered_player != -1) {
            cell = SDLex_GridGetCellRect(&grid1, -1, hovered_player+1);
            SDL_SetRenderDrawColor(renderer, 240, 20, 20, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered character name cell
        if (hovered_character != -1) {
            cell = SDLex_GridGetCellRect(&grid2, 0, hovered_character);
            SDL_SetRenderDrawColor(renderer, 0, 20, 240, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered checkmark cell
        if (hovered_checkmark != -1) {
            cell = SDLex_GridGetCellRect(&grid2, 1, hovered_checkmark);
            SDL_SetRenderDrawColor(renderer, 250, 20, 0, 80);
            SDL_RenderDrawLine(renderer, cell.x, cell.y, cell.x+cell.w, cell.y+cell.h);
            SDL_RenderDrawLine(renderer, cell.x+cell.w, cell.y, cell.x, cell.y+cell.h);
                
        }
        // fill selected item cell
        if (selected_item != -1) {
            cell = SDLex_GridGetCellRect(&grid1, selected_item, 0);
            SDL_SetRenderDrawColor(renderer, 20, 240, 80, 230);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected player name cell
        if (selected_player != -1) {
            cell = SDLex_GridGetCellRect(&grid1, -1, selected_player+1);
            SDL_SetRenderDrawColor(renderer, 240, 20, 20, 200);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected character name cell
        if (selected_character != -1) {
            cell = SDLex_GridGetCellRect(&grid2, 0, selected_character);
            SDL_SetRenderDrawColor(renderer, 50, 50, 150, 180);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected checkmarks
        SDL_SetRenderDrawColor(renderer, 250, 20, 0, 200);
        for (int i = 0; i < 13; ++i) {
            if (checked_characters[i] != -1) {
                cell = SDLex_GridGetCellRect(&grid2, 1, i);
                SDL_RenderDrawLine(renderer, cell.x, cell.y, cell.x+cell.w, cell.y+cell.h);
                SDL_RenderDrawLine(renderer, cell.x+cell.w, cell.y, cell.x, cell.y+cell.h);
            }
        }

        //draw items sprites and text (grid 1)
        for (int i = 0; i <8; ++i) {
            items_sprites[i].scale = (SDL_FPoint){0.4, 0.4};
            SDLex_RenderDrawSprite(renderer, &items_sprites[i]);
            items_sprites[i].scale = (SDL_FPoint){0.25, 0.25};
            SDLex_RenderDrawText(renderer, items_texts[i]);
        }

        // draw characters names and items (grid2)
        for (int i = 0; i < 13; ++i) {
            SDLex_RenderDrawTextAt(renderer, characters_texts[i], (SDL_Point){grid2.position.x+grid2.padding_topleft.x+12, grid2.position.y+grid2.cell_size.y*i+10});
            for (int j = 0; j < 3; ++j) {
                int item = characters_items[i][j];
                if (item != -1) {
                    SDLex_RenderDrawSpriteAt(renderer, &items_sprites[item], (SDL_Point){grid2.position.x+grid2.padding_topleft.x-34*(j+1), grid2.position.y+grid2.cell_size.y*i+3});
                }
            }
        }

        if (!connected)
            SDLex_RenderDrawSprite(renderer, &connect_btn);

        // draw grids
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDLex_RenderDrawGrid(renderer, &grid1);
        SDLex_RenderDrawGrid(renderer, &grid2);

        SDL_RenderPresent(renderer);
        // END RENDERING
        
        // limit FPS
        float elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        SDL_Delay(fmax(0, 16.66 - elapsed_s*1000));
        elapsed_s = (SDL_GetPerformanceCounter() - start_time) / (float)SDL_GetPerformanceFrequency();
        sprintf(window_title, "Sherlock 13 Online ! - FPS : %f - frame duration : %f ms", 1.0/elapsed_s, elapsed_s*1000);
        SDL_SetWindowTitle(window, window_title);
    }

    for (int i = 0 ; i<13; ++i)
        SDL_DestroyTexture(cards_textures[i]);
    for (int i = 0 ; i<8; ++i) {
        SDL_DestroyTexture(items_textures[i]);
        SDLex_DestroyText(items_texts[i]);
    }
    SDL_DestroyTexture(connectbtn_texture);
    SDL_DestroyTexture(gobtn_texture);


    TTF_CloseFont(font_15);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    freeaddrinfo(server_ai);
    close(socket_fd);

    return EXIT_SUCCESS;
}
