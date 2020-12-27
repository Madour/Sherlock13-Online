#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "client/SDLex.h"
#include "common/typedefs.h"

typedef struct Player {
    char name[32];
    int cards[3];
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
    SDLex_Text* digits[12];         // 0 to 9 and * and 1,
    SDLex_Text* wait_players;
    SDLex_Text* who_is_playing;
    SDLex_Text* items_nb[8];        // pointers to digits texts
    SDLex_Text* players_names[4];
    SDLex_Text* character_names[13];
    SDLex_Text* action_description;
};


typedef struct Game {
    // system info
    bool quit;
    bool mouse_click;
    SDL_Point mouse_pos;

    // connections info
    bool connected;
    int players_nb;

    bool started;
    int turn;

    // players info
    Player players[4];

    Player* me;
    int my_index;
    int players_items_count[4][8];

    // game data
    struct Selection selected;
    struct Hovering hovered;
    const struct GameData* data;

    // render info
    SDL_Renderer* renderer;
    TTF_Font* font;
    struct GameTextures textures;
    struct GameSprites sprites;
    struct GameTexts texts;

    SDLex_Grid grid1;
    SDLex_Grid grid2;

} Game;


void Game_init(Game* game, SDL_Renderer* renderer);

void Game_reset(Game* game);

void Game_update(Game* game);

void Game_render(Game* game);

void Game_terminate(Game* game);

#endif//GAME_H
