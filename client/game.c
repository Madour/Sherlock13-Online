#include "client/game.h"


static void Game_InitTextures(Game* game);

static void Game_InitSprites(Game* game);

static void Game_InitTexts(Game* game);

void Game_Init(Game* game, SDL_Renderer* renderer) {
    game->quit = false;
    
    game->mouse_click = false;
    game->mouse_pos.x = game->mouse_pos.y = 0;
    
    game->connected = false;
    game->players_nb = 0;
    game->my_index = 0;

    game->started = false;

    game->selected.item = -1;
    game->selected.player = -1;
    game->selected.character = -1;
    for (int i = 0; i < 13; ++i)
        game->selected.checkmarks[i] = -1;

    // render data
    game->renderer = renderer;
    game->font = TTF_OpenFont("assets/sans.ttf", 15);

    // load textures
    Game_InitTextures(game);

    // create the grids
    game->grid1.position = (SDL_Point){10, 10};
    game->grid1.cell_size = (SDL_Point){60, 50};
    game->grid1.padding_topleft = (SDL_Point){240, 0};
    game->grid1.padding_bottomright = (SDL_Point){0, 0};
    game->grid1.colomns_nb = 8; game->grid1.rows_nb = 5;

    game->grid2.position = (SDL_Point){10, 270};
    game->grid2.cell_size = (SDL_Point){200, 37};
    game->grid2.padding_topleft = (SDL_Point){37*3, 0};
    game->grid2.padding_bottomright = (SDL_Point){40, 0};
    game->grid2.colomns_nb = 1; game->grid2.rows_nb = 13;

    // create the sprites
    Game_InitSprites(game);
    
    // create the texts
    Game_InitTexts(game);
}

void Game_InitTextures(Game* game) {
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

void Game_InitSprites(Game* game) {
    SDL_Rect zero_rect = {0, 0, 0, 0};
    SDL_Point zero_point = {0, 0};
    SDL_FPoint zero_fpoint = {0, 0};
    
    game->sprites.btn_connect = (SDLex_Sprite){
        game->textures.btn_connect,
        zero_rect, 
        (SDL_Point){game->grid1.position.x+30, game->grid1.position.y+2}, 
        (SDL_FPoint){0.9, 0.9}
    };
    
    game->sprites.btn_go = (SDLex_Sprite){game->textures.btn_go, zero_rect, zero_point, zero_fpoint};

    // create player cards sprites
    for (int i = 0; i < 3; ++i) {
        game->sprites.cards[i] = (SDLex_Sprite){
            game->textures.cards[i],
            zero_rect,
            (SDL_Point){750, 110+200*i},
            (SDL_FPoint){0.25, 0.25}
        };
    }

    // create items sprites
    for (int i = 0; i < 8; ++i) {
        SDL_Point pos = {
            game->grid1.position.x+game->grid1.padding_topleft.x+game->grid1.cell_size.x*i+5,
            game->grid1.position.y
        };
        game->sprites.items[i] = (SDLex_Sprite){game->textures.items[i], zero_rect, pos, (SDL_FPoint){0.3, 0.3}};
    }
}

void Game_InitTexts(Game* game) {
    for (int i = 0; i < 8; ++i) {
        if (i < 4)
            game->texts.items_nb[i] = SDLex_CreateText(game->renderer, "8", game->font);
        else if (i == 4)
            game->texts.items_nb[i] = SDLex_CreateText(game->renderer, "4", game->font);
        else
            game->texts.items_nb[i] = SDLex_CreateText(game->renderer, "3", game->font);
        SDL_Point pos = {game->grid1.position.x+game->grid1.padding_topleft.x+game->grid1.cell_size.x*(i+1)-11, game->grid1.position.y+game->grid1.cell_size.y-20};
        SDLex_TextSetPosition(game->texts.items_nb[i], pos.x, pos.y);
    }

    for (int i = 0; i < 13; ++i) {
        char* name = game->data.character_names[i];
        game->texts.character_names[i] = SDLex_CreateText(game->renderer, name, game->font);
        SDL_Point pos = {game->grid2.position.x+game->grid2.padding_topleft.x+12, game->grid2.position.y+game->grid2.cell_size.y*i+10};
        SDLex_TextSetPosition(game->texts.character_names[i], pos.x, pos.y);
    }

    game->texts.wait_players = SDLex_CreateText(game->renderer, "Waiting for 3 players...", game->font);
    SDLex_TextSetPosition(game->texts.wait_players, game->sprites.btn_connect.position.x, game->sprites.btn_connect.position.y+10);

    for (int i = 0; i < 4; ++i) {
        game->texts.player_names[i] = NULL;
    }
}

void Game_Update(Game* game) {
    SDL_Rect cell;
    game->hovered.item = -1;
    game->hovered.player = -1;
    game->hovered.character = -1;
    game->hovered.checkmark = -1;

    // check if hovering/selecting an item cell
    for (int i = 0; i < game->grid1.colomns_nb; ++i) {
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


void Game_Render(Game* game) {
    SDL_Renderer* renderer = game->renderer;

    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    // draw cards
    SDLex_RenderDrawSprite(renderer, &game->sprites.cards[0]);
    SDLex_RenderDrawSprite(renderer, &game->sprites.cards[1]);
    SDLex_RenderDrawSprite(renderer, &game->sprites.cards[2]);

    SDL_Rect cell;
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
        SDL_SetRenderDrawColor(renderer, 20, 240, 80, 230);
        SDL_RenderFillRect(renderer, &cell);
    }
    // fill selected player name cell
    if (game->selected.player != -1) {
        cell = SDLex_GridGetCellRect(&game->grid1, -1, game->selected.player+1);
        SDL_SetRenderDrawColor(renderer, 240, 20, 20, 200);
        SDL_RenderFillRect(renderer, &cell);
    }
    // fill selected character name cell
    if (game->selected.character != -1) {
        cell = SDLex_GridGetCellRect(&game->grid2, 0, game->selected.character);
        SDL_SetRenderDrawColor(renderer, 50, 50, 150, 180);
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


    //draw items sprites and text (grid 1)
    for (int i = 0; i <8; ++i) {
        game->sprites.items[i].scale = (SDL_FPoint){0.4, 0.4};
        SDLex_RenderDrawSprite(renderer, &game->sprites.items[i]);
        game->sprites.items[i].scale = (SDL_FPoint){0.25, 0.25};
        SDLex_RenderDrawText(renderer, game->texts.items_nb[i]);
    }

    // draw characters names and items (grid2)
    for (int i = 0; i < 13; ++i) {
        //SDL_Point pos = {game->grid2.position.x+game->grid2.padding_topleft.x+12, game->grid2.position.y+game->grid2.cell_size.y*i+10};
        SDLex_RenderDrawText(renderer, game->texts.character_names[i]);
        for (int j = 0; j < 3; ++j) {
            int item = *(game->data.character_items + i*3 + j);
            if (item != -1) {
                SDL_Point pos = (SDL_Point){game->grid2.position.x+game->grid2.padding_topleft.x-34*(j+1), game->grid2.position.y+game->grid2.cell_size.y*i+3};
                SDLex_RenderDrawSpriteAt(renderer, &game->sprites.items[item], pos);
            }
        }
    }

    // draw player names
    for (int i = 0; i < 4; ++i) {
        if (game->texts.player_names[i] != NULL) {
            SDL_Point pos = {game->grid1.position.x + 20 , game->grid1.position.y+10+(game->grid1.cell_size.y)*(i+1)};
            SDLex_RenderDrawTextAt(renderer, game->texts.player_names[i], pos);
        }
    }
    
    if (!game->connected) {
        SDLex_RenderDrawSprite(renderer, &game->sprites.btn_connect);
    }
    else if (!game->started) {
        if (game->texts.wait_players != NULL)
            SDLex_RenderDrawText(renderer, game->texts.wait_players);
    } 

    // draw grids
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDLex_RenderDrawGrid(renderer, &game->grid1);
    SDLex_RenderDrawGrid(renderer, &game->grid2);

    SDL_RenderPresent(renderer);
}

void Game_Terminate(Game* game) {
    for (int i = 0; i < 13; ++i) {
        SDL_DestroyTexture(game->textures.cards[i]);
        SDLex_DestroyText(game->texts.character_names[i]);
    }

    for (int i = 0 ; i<8; ++i) {
        SDL_DestroyTexture(game->textures.items[i]);
        SDLex_DestroyText(game->texts.items_nb[i]);
    }

    SDL_DestroyTexture(game->textures.btn_connect);
    SDL_DestroyTexture(game->textures.btn_go);

    for (int i = 0; i < 4; ++i) {
        if (game->texts.player_names[i] != NULL)
            SDLex_DestroyText(game->texts.player_names[i]);
    }
    SDLex_DestroyText(game->texts.wait_players);

    TTF_CloseFont(game->font);
}
