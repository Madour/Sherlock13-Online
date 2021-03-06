/**
 * @file game.c
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

#include "client/game.h"


static void Game_initTextures(Game* game);

static void Game_initSprites(Game* game);

static void Game_initTexts(Game* game);

void Game_init(Game* game, SDL_Renderer* renderer) {
    game->data = &DATA;

    // renderer data
    game->renderer = renderer;
    game->font = TTF_OpenFont("assets/sans.ttf", 15);

    // create the grids
    game->grid1.position = (SDL_Point){10, 10};
    game->grid1.cell_size = (SDL_Point){60, 50};
    game->grid1.padding_topleft = (SDL_Point){240, 0};
    game->grid1.padding_bottomright = (SDL_Point){0, 0};
    game->grid1.columns_nb = 8; game->grid1.rows_nb = 5;

    game->grid2.position = (SDL_Point){10, 270};
    game->grid2.cell_size = (SDL_Point){200, 37};
    game->grid2.padding_topleft = (SDL_Point){37*3, 0};
    game->grid2.padding_bottomright = (SDL_Point){40, 0};
    game->grid2.columns_nb = 1; game->grid2.rows_nb = 13;

    // load textures
    Game_initTextures(game);

    // create the sprites
    Game_initSprites(game);
    
    // create the texts
    Game_initTexts(game);

    Game_reset(game);
}

void Game_reset(Game* game) {
    game->quit = false;
    
    game->mouse_click = false;
    game->mouse_pos.x = game->mouse_pos.y = 0;
    
    game->started = false;
    memset(game->players, 0, sizeof(Player)*4);
    game->players_nb = 0;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 8; ++j)
            game->players_items_count[i][j] = -2;

    game->me = NULL;
    game->my_index = 0;

    game->started = false;
    game->ended = false;
    game->turn = 0;

    memset(&game->selected, -1, sizeof(struct Selection));
    memset(&game->hovered, -1, sizeof(struct Hovering));

    for (int i = 0; i < 4; ++i) {
        if (game->texts.players_names[i] != NULL)
            SDLex_DestroyText(game->texts.players_names[i]);
        game->texts.players_names[i] = NULL;
    }
    if(game->texts.action_description != NULL)
        SDLex_TextSetString(game->texts.action_description, "");
}

void Game_initTextures(Game* game) {
    char buffer[64];
    // load all cards textures
    for (int i = 0; i < 13; ++i) {
        sprintf(buffer, "assets/card_%d.png", i);
        game->textures.cards[i] = SDLex_LoadTextureFromFile(game->renderer, buffer);
    }
    // load all items icons textures
    for (int i = 0; i < 8; ++i) {
        sprintf(buffer, "assets/item_%d.png", i);
        game->textures.items[i] = SDLex_LoadTextureFromFile(game->renderer, buffer);
    }
    // load buttons textures
    game->textures.btn_connect = SDLex_LoadTextureFromFile(game->renderer, "assets/connectbutton.png");
    game->textures.btn_go = SDLex_LoadTextureFromFile(game->renderer, "assets/gobutton.png");
}

void Game_initSprites(Game* game) {
    
    game->sprites.btn_connect = (SDLex_Sprite){
        game->textures.btn_connect,
        {0, 0, 0, 0}, 
        {game->grid1.position.x+30, game->grid1.position.y+2}, 
        {0.9, 0.9}
    };
    
    game->sprites.btn_go = (SDLex_Sprite){game->textures.btn_go, {0, 0, 0, 0}, {490, 290}, {0.5, 0.5}};

    // create player cards sprites
    for (int i = 0; i < 3; ++i) {
        game->sprites.cards[i] = (SDLex_Sprite){
            NULL,
            {0, 0, 0, 0},
            {750, 110+200*i},
            {0.25, 0.25}
        };
    }

    // create items sprites
    for (int i = 0; i < 8; ++i) {
        SDL_Point pos = {
            game->grid1.position.x+game->grid1.padding_topleft.x+game->grid1.cell_size.x*i+5,
            game->grid1.position.y
        };
        game->sprites.items[i] = (SDLex_Sprite){game->textures.items[i], {0, 0, 0, 0}, pos, {0.3, 0.3}};
    }
}

void Game_initTexts(Game* game) {
    SDL_Rect cell;
    for (int i = 0; i < 10; ++i) {
        char tmp[2] = {i+'0', '\0'};
        game->texts.digits[i] = SDLex_CreateText(game->renderer, tmp, game->font);
    }
    game->texts.digits[10] = SDLex_CreateText(game->renderer, "?", game->font);
    game->texts.digits[11] = SDLex_CreateText(game->renderer, "*", game->font);

    // create items number texts
    for (int i = 0; i < 8; ++i) {
        if (i < 4)
            game->texts.items_nb[i] = game->texts.digits[5];
        else if (i == 4)
            game->texts.items_nb[i] = game->texts.digits[4];
        else
            game->texts.items_nb[i] = game->texts.digits[3];
        cell = SDLex_GridGetCellRect(&game->grid1, i, 0);
    }

    // create character names texts
    for (int i = 0; i < 13; ++i) {
        char* name = game->data->characters_names[i];
        game->texts.character_names[i] = SDLex_CreateText(game->renderer, name, game->font);
        cell = SDLex_GridGetCellRect(&game->grid2, 0, i);
        SDLex_TextSetPosition(game->texts.character_names[i], cell.x+12, cell.y+10);
    }

    game->texts.wait_players = SDLex_CreateText(game->renderer, "Waiting for 3 players...", game->font);
    SDLex_TextSetPosition(game->texts.wait_players, game->grid1.position.x+40, game->grid1.position.y+15);

    game->texts.who_is_playing = SDLex_CreateText(game->renderer, "It is someone's turn.", game->font);
    SDLex_TextSetPosition(game->texts.who_is_playing, game->grid1.position.x+40, game->grid1.position.y+15);

    for (int i = 0; i < 4; ++i) {
        game->texts.players_names[i] = NULL;
    }

    game->texts.action_description = SDLex_CreateText(game->renderer, "", game->font);
    SDLex_TextSetPosition(game->texts.action_description, 410, 470);
}

void Game_update(Game* game) {
    // nothing to do if game not started yet
    if (!game->started)
        return;

    SDL_Rect cell;
    game->hovered.item = -1;
    game->hovered.player = -1;
    game->hovered.character = -1;
    game->hovered.checkmark = -1;

    // check if hovering/selecting an item cell
    for (int i = 0; i < game->grid1.columns_nb; ++i) {
        cell = SDLex_GridGetCellRect(&game->grid1, i, 0);
        if (SDL_PointInRect(&game->mouse_pos, &cell)) {
            game->hovered.item = i;
            if (game->mouse_click)
                game->selected.item = game->selected.item == i ? -1 : i;
            break;
        }
    }

    // check if hovering/selecting a player name cell
    for (int i = 1; i < game->grid1.rows_nb; ++i) {
        // prevent selecting my own name 
        if (i == game->my_index+1)
            continue;
        cell = SDLex_GridGetCellRect(&game->grid1, -1, i);
        if (SDL_PointInRect(&game->mouse_pos, &cell)) {
            game->hovered.player = i-1;
            if (game->mouse_click)
                game->selected.player = game->selected.player == i-1 ? -1 : i-1;
            break;
        }
    }
    if (game->selected.item > -1 || game->selected.player > -1)
        game->selected.character = -1;

    // check if hovering/selecting a character name cell
    for (int i = 0; i < game->grid2.rows_nb; ++i) {
        // prevent selecting my own cards
        if (i == game->me->cards[0] || i == game->me->cards[1] || i == game->me->cards[2])
            continue;
        cell = SDLex_GridGetCellRect(&game->grid2, 0, i);
        if (SDL_PointInRect(&game->mouse_pos, &cell)) {
            game->hovered.character = i;
            if (game->mouse_click)
                game->selected.character = game->selected.character == i ? -1 : i;
            break;
        }
    }
    if (game->selected.character > -1)
        game->selected.item = game->selected.player = -1;

    // check if hovering/selecting check mark cell
    for (int i = 0; i < game->grid2.rows_nb; ++i) {
        // prevent unchecking my own cards
        if (i == game->me->cards[0] || i == game->me->cards[1] || i == game->me->cards[2])
            continue;
        cell = SDLex_GridGetCellRect(&game->grid2, 1, i);
        if (SDL_PointInRect(&game->mouse_pos, &cell)) {
            game->hovered.checkmark = i;
            if (game->mouse_click) {
                game->selected.checkmarks[i] *= -1;
            }
            break;
        }
    }
}


void Game_render(Game* game) {
    SDL_Renderer* renderer = game->renderer;

    SDL_SetRenderDrawColor(renderer, 220, 220, 255, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    
    SDL_Rect cell;
    if (!game->ended) {
        // fill hovered item cell
        if (game->hovered.item != -1) {
            cell = SDLex_GridGetCellRect(&game->grid1, game->hovered.item, 0);
            SDL_SetRenderDrawColor(renderer, 20, 240, 80, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered player name cell
        if (game->hovered.player != -1) {
            cell = SDLex_GridGetCellRect(&game->grid1, -1, game->hovered.player+1);
            SDL_SetRenderDrawColor(renderer, 240, 20, 20, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered character name cell
        if (game->hovered.character != -1) {
            cell = SDLex_GridGetCellRect(&game->grid2, 0, game->hovered.character);
            SDL_SetRenderDrawColor(renderer, 0, 20, 240, 80);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill hovered checkmark cell
        if (game->hovered.checkmark != -1) {
            cell = SDLex_GridGetCellRect(&game->grid2, 1, game->hovered.checkmark);
            SDL_SetRenderDrawColor(renderer, 250, 20, 0, 80);
            SDL_RenderDrawLine(renderer, cell.x, cell.y, cell.x+cell.w, cell.y+cell.h);
            SDL_RenderDrawLine(renderer, cell.x+cell.w, cell.y, cell.x, cell.y+cell.h);

        }
        // fill selected item cell
        if (game->selected.item != -1) {
            cell = SDLex_GridGetCellRect(&game->grid1, game->selected.item, 0);
            SDL_SetRenderDrawColor(renderer, 20, 240, 80, 220);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected player name cell
        if (game->selected.player != -1) {
            cell = SDLex_GridGetCellRect(&game->grid1, -1, game->selected.player+1);
            SDL_SetRenderDrawColor(renderer, 240, 20, 20, 180);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected character name cell
        if (game->selected.character != -1) {
            cell = SDLex_GridGetCellRect(&game->grid2, 0, game->selected.character);
            SDL_SetRenderDrawColor(renderer, 100, 100, 180, 180);
            SDL_RenderFillRect(renderer, &cell);
        }
        // fill selected checkmarks
        SDL_SetRenderDrawColor(renderer, 250, 20, 0, 200);
        for (int i = 0; i < 13; ++i) {
            if (game->selected.checkmarks[i] != -1) {
                cell = SDLex_GridGetCellRect(&game->grid2, 1, i);
                SDL_RenderDrawLine(renderer, cell.x, cell.y, cell.x+cell.w, cell.y+cell.h);
                SDL_RenderDrawLine(renderer, cell.x+cell.w, cell.y, cell.x, cell.y+cell.h);
            }
        }
    }
    //draw items sprites and text (grid 1)
    for (int i = 0; i <8; ++i) {
        game->sprites.items[i].scale = (SDL_FPoint){0.4, 0.4};
        SDLex_RenderDrawSprite(renderer, &game->sprites.items[i]);
        game->sprites.items[i].scale = (SDL_FPoint){0.25, 0.25};
        cell = SDLex_GridGetCellRect(&game->grid1, i, 0);
        SDLex_RenderDrawTextAt(renderer, game->texts.items_nb[i], cell.x+49, cell.y+30);
    }
    // draw characters names and items (grid2)
    for (int i = 0; i < 13; ++i) {
        SDLex_RenderDrawText(renderer, game->texts.character_names[i]);
        for (int j = 0; j < 3; ++j) {
            int item = game->data->characters_items[i][j];
            if (item != -1) {
                cell = SDLex_GridGetCellRect(&game->grid2, 0, i);
                SDLex_RenderDrawSpriteAt(renderer, &game->sprites.items[item], cell.x-34*(j+1), cell.y+3);
            }
        }
    }

    if (!game->connected) {
        SDLex_RenderDrawSprite(renderer, &game->sprites.btn_connect);
    }
    else if (!game->started) {
        if (game->texts.wait_players != NULL)
            SDLex_RenderDrawText(renderer, game->texts.wait_players);
    }
    else { // connected and game started
        cell = SDLex_GridGetCellRect(&game->grid1, -1, game->my_index+1);
        cell = (SDL_Rect){cell.x, cell.y, SDLex_GridGetSize(&game->grid1).x, game->grid1.cell_size.y};
        // fill in yellow my row in the grid 
        SDL_SetRenderDrawColor(renderer, 200, 240, 80, 200);
        SDL_RenderFillRect(renderer, &cell);
        SDLex_RenderDrawText(renderer, game->texts.who_is_playing);

        // draw players names
        for (int i = 0; i < 4; ++i) {
            if (game->texts.players_names[i] != NULL) {
                SDLex_RenderDrawText(renderer, game->texts.players_names[i]);
                // draw players items number
                for (int j = 0; j < 8; ++j) {
                    SDL_Rect cell = SDLex_GridGetCellRect(&game->grid1, j, i+1);
                    int digit = game->players_items_count[i][j];
                    if (digit < 0) digit += 12;
                        SDLex_RenderDrawTextAt(renderer, game->texts.digits[digit], cell.x+cell.w/2-5, cell.y+cell.h/2-10);
                }
            }
        }

        // draw cards
        for (int i = 0; i < 3; ++i) 
            if (game->sprites.cards[i].texture)
               SDLex_RenderDrawSprite(renderer, &game->sprites.cards[i]);
        // draw go button when it is my turn to play or when game ended to replay
        if (game->ended || game->turn == game->my_index) {
            SDLex_RenderDrawSprite(renderer, &game->sprites.btn_go);
        }
        // draw last action description
        SDLex_RenderDrawText(renderer, game->texts.action_description);
    }

    // draw grids
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDLex_RenderDrawGrid(renderer, &game->grid1);
    SDLex_RenderDrawGrid(renderer, &game->grid2);

    SDL_RenderPresent(renderer);
}

void Game_terminate(Game* game) {
    SDL_DestroyTexture(game->textures.btn_connect);
    SDL_DestroyTexture(game->textures.btn_go);
    
    for (int i = 0; i < 13; ++i) {
        SDL_DestroyTexture(game->textures.cards[i]);
        SDLex_DestroyText(game->texts.character_names[i]);
    }

    for (int i = 0 ; i<8; ++i) {
        SDL_DestroyTexture(game->textures.items[i]);
    }

    for (int i = 0; i < 12; ++i) 
        SDLex_DestroyText(game->texts.digits[i]);

    for (int i = 0; i < 4; ++i) {
        if (game->texts.players_names[i] != NULL)
            SDLex_DestroyText(game->texts.players_names[i]);
    }
    SDLex_DestroyText(game->texts.wait_players);
    SDLex_DestroyText(game->texts.who_is_playing);
    SDLex_DestroyText(game->texts.action_description);

    TTF_CloseFont(game->font);
}
