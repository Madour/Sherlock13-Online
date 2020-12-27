#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <stdbool.h>

typedef enum MsgCode {
    WaitingPlayers='W',
    GameStart='S',
    DistribCards='C',

    PlayerTurn='T',

    AskItem='I',
    AnswerItem='J',

    AskPlayer='P',
    AnswerPlayer='R',

    GuessSuspect='G',
    AnswerSuspect='H',

    Victory='V',
    QuitLobby='Q',
} MsgCode;

struct GameData {
    char* characters_names[13];
    int characters_items[13][3];
    char* items_names[8];
};

extern const struct GameData DATA;

void deb_log(const char* format, ...);

#endif//TYPEDEFS_H 
