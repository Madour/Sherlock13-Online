#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "SDLex.h"

typedef struct Player {
    char name[32];
    char index;
} Player;

struct Selection {
    int item;
    int player;
    int character;
    int checkmarks[13];
};

struct Hovering {
    int item;
    int player;
    int character;
    int checkmark;
};

struct GameTextures {
    SDL_Texture* cards[13];
    SDL_Texture* items[8];
    SDL_Texture* btn_connect;
    SDL_Texture* btn_go;
};

struct GameSprites {
    SDLex_Sprite cards[3];
    SDLex_Sprite items[8];

    SDLex_Sprite btn_connect;
    SDLex_Sprite btn_go;
};

struct GameTexts {
    SDLex_Text* wait_players;
    SDLex_Text* items_nb[8];
    SDLex_Text* player_names[4];
    SDLex_Text* character_names[13];
};

struct GameData {
    char** character_names;
    int* character_items;
};

typedef struct Game {
    // system info
    bool quit;
    bool mouse_click;
    SDL_Point mouse_pos;

    // connections info
    bool connected;
    
    int players_nb;
    int my_index;

    bool started;

    // player info
    Player players[4];

    // game data
    struct Selection selected;
    struct Hovering hovered;
    struct GameData data;

    // render info
    SDL_Renderer* renderer;
    TTF_Font* font;
    struct GameTextures textures;
    struct GameSprites sprites;
    struct GameTexts texts;

    SDLex_Grid grid1;
    SDLex_Grid grid2;

} Game;


void Game_Init(Game* game, SDL_Renderer* renderer);

void Game_Update(Game* game);

void Game_Render(Game* game);

void Game_Terminate(Game* game);

#endif//GAME_H