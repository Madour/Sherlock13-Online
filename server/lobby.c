#include <stdio.h>
#include <unistd.h>

#include "server/lobby.h"


void broadcast(Lobby* lobby, char* msg, unsigned int size) {
    printf("[INFO] Broadcasting message to lobby %d : \"%s\"\n", lobby->index, msg);

    for (int i = 0; i < lobby->players_nb; ++i) {
        printf("     > Message sent to player n°%d (%s)\n", i, lobby->players[i].name);
        write(lobby->players[i].client.sfd, msg, sizeof(char)*size);
    }
    printf(" \n");
}
