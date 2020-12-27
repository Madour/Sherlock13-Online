/*
 * @file SDLex.h
 * @author Modar Nasser
 * @brief Some useful functions to extend SDL2
 * @date 2020-11-04
 */

#ifndef SDL_EX_H
#define SDL_EX_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

extern const SDL_Color SDL_ex_BLACK;
extern const SDL_Color SDL_ex_WHITE;
extern const SDL_Color SDL_ex_RED;
extern const SDL_Color SDL_ex_GREEN;
extern const SDL_Color SDL_ex_BLUE;
extern const SDL_Color SDL_ex_YELLOW;
extern const SDL_Color SDL_ex_MAGENTA;
extern const SDL_Color SDL_ex_CYAN;

//---------------TEXTURE---------------------------//

/**
 * @brief Load a Texture from a file
 * 
 * @param renderer Renderer used 
 * @param filename 
 * @return SDL_Texture* Pointer to the loaded texture 
 */
SDL_Texture* SDLex_LoadTextureFromFile(SDL_Renderer* renderer, const char* filename);

/**
 * @brief Returns the Size of a given SDL texture
 * 
 * @param texture 
 * @return SDL_Point 
 */
SDL_Point SDLex_GetTextureSize(SDL_Texture* texture);

//------------------------------------------------//


//---------------SPRITE---------------------------//

/**
 * @brief Represents a Sprite
 * Contains minimal needed information to render a Sprite from a Texture
 * 
 */
typedef struct SDLex_Sprite {
    SDL_Texture* texture;
    SDL_Rect texture_rect;
    SDL_Point position;
    SDL_FPoint scale;
} SDLex_Sprite;

/**
 * @brief Returns the global bounds of a SDLex sprite
 * 
 * @param sprite 
 * @return SDL_FRect Sprite bounds
 */
SDL_Rect SDLex_SpriteGetBounds(SDLex_Sprite* sprite);

/**
 * @brief Render a SDLex sprite on a SDL renderer
 * 
 * @param renderer 
 * @param sprite 
 */
void SDLex_RenderDrawSprite(SDL_Renderer* renderer, SDLex_Sprite* sprite);
void SDLex_RenderDrawSpriteAt(SDL_Renderer* renderer, SDLex_Sprite* sprite, int x, int y);

//------------------------------------------------//


//---------------TEXT---------------------------//

typedef struct SDLex_Text {
    SDLex_Sprite drawable;
    TTF_Font* font;
    SDL_Renderer* renderer;
} SDLex_Text;


SDLex_Text* SDLex_CreateText(SDL_Renderer* renderer, char* string, TTF_Font* font);

void SDLex_TextSetPosition(SDLex_Text* text, int x, int y);

void SDLex_TextSetString(SDLex_Text* text, char* string);

void SDLex_RenderDrawText(SDL_Renderer*, SDLex_Text* text);

void SDLex_RenderDrawTextAt(SDL_Renderer*, SDLex_Text* text, int x, int y);

void SDLex_DestroyText(SDLex_Text* text);

//------------------------------------------------//


//---------------GRID---------------------------//

typedef struct SDLex_Grid {
    SDL_Point position;
    SDL_Point cell_size;
    int columns_nb;
    int rows_nb;
    SDL_Point padding_topleft;
    SDL_Point padding_bottomright;
} SDLex_Grid;


SDL_Point SDLex_GridGetSize(SDLex_Grid* grid);

SDL_Rect SDLex_GridGetOutline(SDLex_Grid* grid);

void SDLex_RenderDrawGrid(SDL_Renderer*, SDLex_Grid* grid);

SDL_Rect SDLex_GridGetCellRect(SDLex_Grid* grid, int cx, int cy);

//------------------------------------------------//

#endif//SDL_EX_H
