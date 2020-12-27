
/**
 * @file SDLex.c
 * @author Modar Nasser
 * @date 2020-11-04
 */

#include "SDLex.h"

// common colors
const SDL_Color SDL_ex_BLACK = {0, 0, 0};
const SDL_Color SDL_ex_WHITE = {255, 255, 255};
const SDL_Color SDL_ex_RED = {255, 0, 0};
const SDL_Color SDL_ex_GREEN = {0, 255, 0};
const SDL_Color SDL_ex_BLUE = {0, 0, 255};
const SDL_Color SDL_ex_YELLOW = {255, 255, 0};
const SDL_Color SDL_ex_MAGENTA = {255, 0, 255};
const SDL_Color SDL_ex_CYAN = {0, 255, 255};


SDL_Texture* SDLex_LoadTextureFromFile(SDL_Renderer* renderer, const char* filename) {
    SDL_Texture* texture = IMG_LoadTexture(renderer, filename);

    if ( texture == NULL ) {
        printf("Failed to load texture %s : %s\n", filename, SDL_GetError());
        return NULL;
    }
 
    return texture;
}

SDL_Point SDLex_GetTextureSize(SDL_Texture* texture) {
    SDL_Point texture_size;
    SDL_QueryTexture(texture, NULL, NULL, &texture_size.x, &texture_size.y);
    return texture_size;
}

SDL_Rect SDLex_SpriteGetBounds(SDLex_Sprite* sprite) {
    return (SDL_Rect){
        sprite->position.x,
        sprite->position.y,
        (int)(sprite->texture_rect.w*sprite->scale.x),
        (int)(sprite->texture_rect.h*sprite->scale.y)
    };
}


void SDLex_RenderDrawSprite(SDL_Renderer* renderer, SDLex_Sprite* sprite) {
    int tw = sprite->texture_rect.w;
    int th = sprite->texture_rect.h;
    int tx = sprite->texture_rect.x;
    int ty = sprite->texture_rect.y; 
    float scalex = sprite->scale.x;
    float scaley = sprite->scale.y;
    if (tw == 0 || th == 0) {
        SDL_QueryTexture(sprite->texture, NULL, NULL, &tw, &th);
        sprite->texture_rect.w = tw;
        sprite->texture_rect.h = th;
    }
    if (scalex <= 0.0001 && scaley <= 0.0001) {
        scalex = scaley = 1.;
    }

    SDL_Rect source = {tx, ty, tw, th};
    SDL_FRect dest = {sprite->position.x, sprite->position.y, tw*scalex, th*scaley};
    
    SDL_RenderCopyF(renderer, sprite->texture, &source, &dest);
}

void SDLex_RenderDrawSpriteAt(SDL_Renderer* renderer, SDLex_Sprite* sprite, int x, int y) {
    int tw = sprite->texture_rect.w;
    int th = sprite->texture_rect.h;
    int tx = sprite->texture_rect.x;
    int ty = sprite->texture_rect.y; 
    float scalex = sprite->scale.x;
    float scaley = sprite->scale.y;
    if (tw == 0 || th == 0) {
        SDL_QueryTexture(sprite->texture, NULL, NULL, &tw, &th);
        sprite->texture_rect.w = tw;
        sprite->texture_rect.h = th;
    }
    if (scalex <= 0.0001 && scaley <= 0.0001) {
        scalex = scaley = 1.;
    }

    SDL_Rect source = {tx, ty, tw, th};
    SDL_FRect dest = {x, y, tw*scalex, th*scaley};
    
    SDL_RenderCopyF(renderer, sprite->texture, &source, &dest);
}

SDLex_Text* SDLex_CreateText(SDL_Renderer* renderer, char* string, TTF_Font* font) {
    SDL_Surface* text_surf = TTF_RenderText_Blended_Wrapped(font, string, (SDL_Color){0, 0, 0, 0}, 300);
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surf);
    SDL_FreeSurface(text_surf);
    
    SDLex_Text* new_text = malloc(sizeof(SDLex_Text));
    new_text->drawable = (SDLex_Sprite){text_tex, {0, 0, 0, 0}, {0, 0}, {1, 1}};
    new_text->font = font;
    new_text->renderer = renderer;

    return new_text;
}


void SDLex_TextSetPosition(SDLex_Text* text, int x, int y) {
    text->drawable.position.x = x;
    text->drawable.position.y = y;
}

void SDLex_TextSetString(SDLex_Text* text, char* string) {
    SDL_Surface* text_surf = TTF_RenderText_Blended_Wrapped(text->font, string, (SDL_Color){0, 0, 0, 0}, 300);
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(text->renderer, text_surf);
    SDL_FreeSurface(text_surf);

    if (text->drawable.texture != NULL)
        SDL_DestroyTexture(text->drawable.texture);
    text->drawable.texture = text_tex;
    text->drawable.texture_rect.w = 0;
    text->drawable.texture_rect.h = 0;
}

void SDLex_RenderDrawText(SDL_Renderer* renderer, SDLex_Text* text) {
    SDLex_RenderDrawSprite(renderer, &text->drawable);
    text->renderer = renderer;
}

void SDLex_RenderDrawTextAt(SDL_Renderer* renderer, SDLex_Text* text, int x, int y) {
    SDLex_RenderDrawSpriteAt(renderer, &text->drawable, x, y);
    text->renderer = renderer;
}

void SDLex_DestroyText(SDLex_Text* text) {
    SDL_DestroyTexture(text->drawable.texture);
    free(text);
}

SDL_Point SDLex_GridGetSize(SDLex_Grid* grid) {
    return (SDL_Point) {
        grid->padding_topleft.x + grid->cell_size.x*grid->columns_nb + grid->padding_bottomright.x,
        grid->padding_topleft.y + grid->cell_size.y*grid->rows_nb + grid->padding_bottomright.y
    };
}

SDL_Rect SDLex_GridGetOutline(SDLex_Grid* grid) {
    SDL_Point grid_size = SDLex_GridGetSize(grid);
    return (SDL_Rect) {
        grid->position.x, grid->position.y,
        grid_size.x, grid_size.y
    };
}

void SDLex_RenderDrawGrid(SDL_Renderer* renderer, SDLex_Grid* g) {
    SDL_Rect g_outline = SDLex_GridGetOutline(g);
    SDL_RenderDrawRect(renderer, &g_outline);
    SDL_Point g_size = SDLex_GridGetSize(g);
    
    // vertical lines
    int stop = g->padding_bottomright.x > 0 ? g->columns_nb+1 : g->columns_nb;
    for (int i = 0; i < stop; ++i)
        SDL_RenderDrawLine(renderer, g->position.x + g->padding_topleft.x+g->cell_size.x*i, g->position.y, 
                                     g->position.x + g->padding_topleft.x+g->cell_size.x*i, g->position.y+g_size.y);
    
    // horizontal lines
    stop = g->padding_bottomright.y > 0 ? g->rows_nb+1 : g->rows_nb;
    for (int i = 0; i < stop; ++i) 
        SDL_RenderDrawLine(renderer, g->position.x,          g->position.y + g->padding_topleft.y+g->cell_size.y*i,
                                     g->position.x+g_size.x, g->position.y + g->padding_topleft.y+g->cell_size.y*i);
        
}

SDL_Rect SDLex_GridGetCellRect(SDLex_Grid* g, int cx, int cy) {
    int x, y, w, h;

    if (cx == -1) {
        x = g->position.x;
        w = g->padding_topleft.x;
    }
    else if (cx < g->columns_nb) {
        x = g->position.x + g->padding_topleft.x + cx*g->cell_size.x;
        w = g->cell_size.x;
    }
    else {
        x = g->position.x + g->padding_topleft.x + g->columns_nb*g->cell_size.x;
        w = g->padding_bottomright.x;
    }

    if (cy == -1) {
        y = g->position.y;
        h = g->padding_topleft.y;
    }
    else if (cy < g->rows_nb) {
        y = g->position.y + g->padding_topleft.y + cy*g->cell_size.y;
        h = g->cell_size.y;
    }
    else {
        y = g->position.y + g->padding_topleft.y + g->columns_nb*g->cell_size.y;
        h = g->padding_bottomright.y;
    }

    return (SDL_Rect){x, y, w, h};

}
