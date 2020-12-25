#ifndef TYPEDEFS_H
#define TYPEDEFS_H

typedef enum MsgCode {
    WaitingPlayers='W',
    GameStart='S',
    DistribCards='C',
    PlayerTurn='P',
    QuitLobby='Q',
} MsgCode;

struct GameData {
    char* character_names[13];
    int character_items[13][3];
};

extern const struct GameData DATA;

#endif//TYPEDEFS_H 
