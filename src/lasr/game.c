#include "game.h"
#include "src/gui/app_window.h"
#include "src/timer.h"

static int no_op(lua_State* L)
{
    return 0;
}

/**
 * @brief Creates the global game object for lua scripts.
 * The values exposed are read only. Trying to edit the game
 * object in lua results in a no-op.
 *
 * @param L The current lua state.
 */
void lasr_game_register(lua_State* L)
{
    // Keep the copied values in a hidden table so all writes are ignored.
    lua_newuserdatauv(L, 0, 0);
    lua_newtable(L);
    lua_createtable(L, 0, 3);

    LSAppWindow* win = ls_get_main_app_window();
    if (win && win->game) {
        lua_pushinteger(L, win->game->start_delay);
        lua_setfield(L, -2, "start_delay");
        lua_pushstring(L, win->game->name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, win->game->category);
        lua_setfield(L, -2, "category");
    } else {
        lua_pushnil(L);
        lua_setfield(L, -2, "start_delay");
        lua_pushnil(L);
        lua_setfield(L, -2, "name");
        lua_pushnil(L);
        lua_setfield(L, -2, "category");
    }

    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, no_op);
    lua_setfield(L, -2, "__newindex");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "game");
}
