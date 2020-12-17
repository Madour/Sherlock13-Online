gcc -Wall -Wpedentic -I. -o game main.c utils.c SDLex.c -lSDL2 -lSDL2_image -lSDL2_ttf -lm -lpthread
gcc -Wall -Wpedentic -I. -o server server.c utils.c -lpthread

