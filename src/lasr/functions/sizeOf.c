#include "sizeOf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * The "sizeOf" Lua AutoSplitter Runtime function
 *
 * Takes the type and returns the size it occupies, in bytes.
 *
 * @param L The Lua State
 */
int size_of(lua_State* L)
{
    if (!lua_isstring(L, 1)) {
        printf("The first argument must be a string defining the type to size");
        return 0;
    }
    const char* type_to_size = lua_tostring(L, 1);
    int size_of_type = 0;
    if (strcmp(type_to_size, "sbyte") == 0) {
        size_of_type = sizeof(int8_t);
    } else if (strcmp(type_to_size, "byte") == 0) {
        size_of_type = sizeof(uint8_t);
    } else if (strcmp(type_to_size, "short") == 0) {
        size_of_type = sizeof(int16_t);
    } else if (strcmp(type_to_size, "ushort") == 0) {
        size_of_type = sizeof(uint16_t);
    } else if (strcmp(type_to_size, "int") == 0) {
        size_of_type = sizeof(int32_t);
    } else if (strcmp(type_to_size, "uint") == 0) {
        size_of_type = sizeof(uint32_t);
    } else if (strcmp(type_to_size, "long") == 0) {
        size_of_type = sizeof(int64_t);
    } else if (strcmp(type_to_size, "ulong") == 0) {
        size_of_type = sizeof(uint64_t);
    } else if (strcmp(type_to_size, "float") == 0) {
        size_of_type = sizeof(float);
    } else if (strcmp(type_to_size, "double") == 0) {
        size_of_type = sizeof(double);
    } else if (strcmp(type_to_size, "bool") == 0) {
        size_of_type = sizeof(bool);
    } else if (strstr(type_to_size, "string") != NULL) {
        int buffer_size = atoi(type_to_size + 6);
        if (buffer_size < 2) {
            printf("Invalid string size, please read documentation");
            return 0;
        }
        size_of_type = sizeof(char) * buffer_size;
    } else if (strstr(type_to_size, "byte")) {
        int array_size = atoi(type_to_size + 4);
        if (array_size < 1) {
            printf("Invalid byte array size, please read documentation");
            return 0;
        }
        size_of_type = sizeof(uint8_t) * array_size;
    } else {
        // Error handling
        printf("Cannot find size of type %s", type_to_size);
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, size_of_type);
    return 1;
}
