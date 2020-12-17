/**
 * @file utils.c
 * @author Modar Nasser
 * @brief Utilities functions
 * @date 2020-11-11
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <errno.h>
#include <limits.h>
#include <string.h>
#include "utils.h"

int isUInt(char* str) {
    errno = 0;
    char* endptr;
    strtoul(str, &endptr, 10);
    return !((str == endptr) || (errno == ERANGE) || (*endptr != '\0'));
}

int splitString(char* separator, char* string, char** output) {
    int argn = 0;
    char* token;
    char* rest = string;
    while( (token = strtok_r(rest, separator, &rest)) )
        output[argn++] = token;
    return argn;
}
