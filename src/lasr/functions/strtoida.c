#include "strtoida.h"
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Converts a ASCII string into an IDA-compliant string
 *
 * @param L The lua stack
 */
int str2ida(lua_State* L)
{
    if (lua_gettop(L) == 0) {
        printf("[str2ida] One argument is required: the string to translate");
        lua_pushnil(L);
        return 1;
    }

    if (!lua_isstring(L, 1)) {
        printf("[str2ida] The first argument must be a string.");
        lua_pushnil(L);
        return 1;
    }

    const char* starting_string = lua_tostring(L, 1);
    const unsigned int starting_string_length = strlen(starting_string);
    // 2 hex digits per character + "n-1" spaces in between
    // + 1 at the last for loop, replaced by \0 + 1 for the natural \0 of strcat
    const unsigned int objective_string_length = 3 * starting_string_length + 1;
    char* objective_string = malloc(objective_string_length * sizeof(char));
    if (!objective_string) {
        printf("[str2ida] Cannot allocate memory for the string conversion.");
        lua_pushnil(L);
        return 1;
    }
    objective_string[0] = '\0';
    // 2 hex digits + one for \0
    char tmp_buf[3];

    for (unsigned int i = 0; i < starting_string_length; i++) {
        snprintf(tmp_buf, 3, "%02X", (unsigned char)starting_string[i]);
        strcat(objective_string, tmp_buf);
        strcat(objective_string, " ");
    }
    objective_string[objective_string_length - 1] = '\0';
    lua_pushstring(L, objective_string);
    free(objective_string);
    return 1;
}
