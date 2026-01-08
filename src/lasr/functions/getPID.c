#include "getPID.h"

#include "../utils.h"

/**
 * Returns the Pid of the game process to the Lua Auto Splitter.
 *
 * @param L the Lua state.
 *
 * @return Always 1.
 */
int getPID(lua_State* L)
{
    lua_pushinteger(L, process.pid);
    return 1;
}
