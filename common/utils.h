/**
 * @file utils.h
 * @author Modar Nasser
 * @brief Utilities functions
 * @date 2020-11-11
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef UTILS_H
#define UTILS_h

#include <stdlib.h>


/**
 * @brief Check if the string is a correct numerical value
 * 
 * @param str String to check
 * @return int 1 if true, 0 if false
 */
int isUInt(char* str);

/**
 * @brief Split a given string by the separator char. Output stores the resulting array
 * 
 * @param separator 
 * @param string 
 * @param output 
 * @return int Returns the number of strings in the ouput array
 */
int splitString(char* separator, char* string, char** output);

#endif//UTILS_H
