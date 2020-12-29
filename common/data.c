/**
 * @file data.c
 * @author Modar Nasser
 * @copyright Copyright (c) 2020
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "common/data.h"

const struct GameData DATA = {
    {
        "Sebastian Moran",
        "Irene Adler",
        "Inspector Lestrade",
        "Inspector Gregson",
        "Inspector Baynes",
        "Inspector Bradstreet",
        "Inspector Hopkins",
        "Sherlock Holmes",
        "John Watson",
        "Mycroft Holmes",
        "Mrs. Hudson",
        "Mary Morstan",
        "James Moriarty"
    },
    {
        {2, 7,-1},  // seb moran
        {5, 1, 7},  // iren adler
        {4, 6, 3},  // insp lestrade
        {4, 2, 3},  // insp gregson
        {1, 3,-1},  // insp baynes
        {2, 3,-1},  // insp bradstreet
        {6, 0, 3},  // insp hopkins
        {2, 1, 0},  // sherlock
        {2, 6, 0},  // watson
        {4, 1, 0},  // mycroft
        {5, 0,-1},  // hudson
        {5, 4,-1},  // mary morstan
        {7, 1,-1},  // james moriarty
    },
    {
        "Smoking pipe",
        "Lightbulb",
        "Fist",
        "Crown",
        "Book",
        "Necklace",
        "Eye",
        "Skull"
    }
};

bool debug = true;

void deb_log(const char* format, ...) {
    if (debug) {
        char tmp[256];
        strcpy(tmp, format);
        strcat(tmp, "\n");
        va_list args;
        va_start(args, tmp);
        vprintf(tmp, args);
        va_end(args);
    }
}
