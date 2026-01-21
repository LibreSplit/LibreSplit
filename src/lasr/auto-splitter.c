/** \file auto-splitter.c
 *
 * Implementation of the auto splitter Lua Runtime
 */
#include "auto-splitter.h"

#include "./maps/maps.h"
#include "functions.h"
#include "utils.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

char auto_splitter_file[PATH_MAX]; /*!< The loaded auto splitter file path */
int refresh_rate = 60; /*!< The Auto Splitter's refresh rate applied */
bool use_game_time = false; /*!< Enables IGT */
atomic_bool update_game_time = false; /*!< True if the auto splitter is requesting the game time to be updated */
atomic_llong game_time_value = 0; /*!< The in-game time value, in milliseconds */

/**
 * Defines the behaviour of the map cache.
 *
 * 0=off, 1=current cycle, +1=multiple cycles
 */
int maps_cache_cycles = 1;
/**
 * Same as `maps_cache_cycles` but this one represents the current value
 * that changes on each cycle rather than the reference from the script
 */
int maps_cache_cycles_value = 1; /*!< The number of cycles the cache is active for */

atomic_bool auto_splitter_enabled = true; /*!< Defines if the auto splitter is enabled */
atomic_bool auto_splitter_running = false; /*!< Defines if the auto splitter is running */
atomic_bool call_start = false; /*!< True if the auto splitter is requesting for a run to start */
atomic_bool run_started = false; /*!< Defines if a run is started */
atomic_bool run_finished = false; // Disallows starting the timer again after finishing until reset
atomic_bool call_split = false; /*!< True if the auto splitter is requesting to split */
atomic_bool toggle_loading = false;
atomic_bool call_reset = false; /*!< True if the auto splitter is requesting a run reset */
bool prev_is_loading; /*!< The previous frame "is_loading" state */

/**
 * Disable possibly dangerous functions in LASR.
 */
static const char* disabled_functions[] = {
    "collectgarbage",
    "dofile",
    "getmetatable",
    "setmetatable",
    "getfenv",
    "setfenv",
    "load",
    "loadfile",
    "loadstring",
    "rawequal",
    "rawget",
    "rawset",
    "module",
    "require",
    "newproxy",
    NULL
};

/**
 * Check if the game process exists and is running.
 *
 * @returns Zero if the process is not running, non-zero if it is.
 */
int process_exists()
{
    int result = kill(process.pid, 0);
    return result == 0;
}

/**
 * Lua libraries to enable in LASR
 */
static const luaL_Reg lj_lib_load[] = {
    { "", luaopen_base },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_BITLIBNAME, luaopen_bit },
    { LUA_JITLIBNAME, luaopen_jit },
    { NULL, NULL }
};

/**
 * Additional functions for the Lua Auto Split Runtime
 *
 * Must be NULL-terminated.
 */
static const lasr_function luac_functions[] = {
    { "process", find_process_id },
    { "getBaseAddress", getBaseAddress },
    { "readAddress", readAddress },
    { "sizeOf", size_of },
    { "sig_scan", perform_sig_scan },
    { "getPID", getPID },
    { "getModuleSize", getModuleSize },
    { "shallow_copy_tbl", shallow_copy_tbl },
    { "print_tbl", print_tbl },
    { "b_and", b_and },
    { "b_or", b_or },
    { "b_xor", b_xor },
    { "b_not", b_not },
    { "b_lshift", b_lshift },
    { "b_rshift", b_rshift },
    { "getMaps", getMaps },
    { NULL, NULL }
};

/**
 * Registers the Lua Auto Split Runtime functions.
 *
 * @param L The lua Stack
 * @param functions The array of name/function pairs to register.
 */
void push_lasr_functions(lua_State* L, const lasr_function* functions)
{
    for (int i = 0; functions[i].function_name != NULL; i++) {
        lua_pushcfunction(L, functions[i].function_ptr);
        lua_setglobal(L, functions[i].function_name);
    }
}

/**
 * Override of the standard openlibs functions to open only a subset
 * of libraries in the Lua Runtime.
 *
 * @param L The lua Stack
 */
LUALIB_API void luaL_openlibs(lua_State* L)
{
    const luaL_Reg* lib;
    for (lib = lj_lib_load; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

/**
 * Disables possibly dangerous functions in the Lua Runtime.
 *
 * @param L The Lua Stack
 * @param functions An array of strings, defining the functions to disable.
 */
void disable_functions(lua_State* L, const char** functions)
{
    for (int i = 0; functions[i] != NULL; i++) {
        lua_pushnil(L);
        lua_setglobal(L, functions[i]);
    }
}

/**
    Generic function to call lua functions
    Signatures are something like `disb>s`
    1. d = double
    2. i = int
    3. s = string
    4. b = boolean
    5. > = return separator

    Example: `call_va("functionName", "dd>d", x, y, &z);`
    Calls "functionName" with two doubles as parameters (x and y), returning a double in z
*/
bool call_va(lua_State* L, const char* func, const char* sig, ...)
{
    va_list vl;
    int narg, nres; /* number of arguments and results */

    va_start(vl, sig);
    lua_getglobal(L, func); /* get function */

    /* push arguments */
    narg = 0;
    while (*sig) { /* push arguments */
        switch (*sig++) {
            case 'd': /* double argument */
                lua_pushnumber(L, va_arg(vl, double));
                break;

            case 'i': /* int argument */
                lua_pushnumber(L, va_arg(vl, int));
                break;

            case 's': /* string argument */
                lua_pushstring(L, va_arg(vl, char*));
                break;

            case 'b':
                lua_pushboolean(L, va_arg(vl, int));
                break;

            case '>':
                break;

            default:
                printf("invalid option (%c)\n", *(sig - 1));
                return false;
        }
        if (*(sig - 1) == '>')
            break;
        narg++;
        luaL_checkstack(L, 1, "too many arguments");
    }

    /* do the call */
    nres = strlen(sig); /* number of expected results */
    if (lua_pcall(L, narg, nres, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        printf("error running function '%s': %s\n", func, err);
        va_end(vl);
        return false;
    }

    /* retrieve results */
    nres = -nres; /* stack index of first result */
    /* check if there's a return value */
    if (!lua_isnil(L, nres)) {
        while (*sig) { /* get results */
            switch (*sig++) {
                case 'd': /* double result */
                    if (!lua_isnumber(L, nres)) {
                        printf("function '%s' wrong result type, expected double\n", func);
                        return false;
                    }
                    *va_arg(vl, double*) = lua_tonumber(L, nres);
                    break;

                case 'i': /* int result */
                    if (!lua_isnumber(L, nres)) {
                        printf("function '%s' wrong result type, expected int\n", func);
                        return false;
                    }
                    *va_arg(vl, int*) = lua_tointeger(L, nres);
                    break;

                case 's': /* string result */
                    if (!lua_isstring(L, nres)) {
                        printf("function '%s' wrong result type, expected string\n", func);
                        return false;
                    }
                    *va_arg(vl, const char**) = lua_tostring(L, nres);
                    break;

                case 'b':
                    if (!lua_isboolean(L, nres)) {
                        printf("function '%s' wrong result type, expected boolean\n", func);
                        return false;
                    }
                    *va_arg(vl, bool*) = lua_toboolean(L, nres);
                    break;

                default:
                    printf("invalid option (%c)\n", *(sig - 1));
                    return false;
            }
            nres++;
        }
    } else {
        va_end(vl);
        return false;
    }
    va_end(vl);
    return true;
}

/**
 * The startup() LASR function.
 *
 * Executes the code in the startup() function of the auto splitter,
 * setting the internal parameters for the execution of the autosplitter.
 *
 * @param L The Lua State
 */
void startup(lua_State* L)
{
    lua_getglobal(L, "startup");
    lua_pcall(L, 0, 0, 0);

    lua_getglobal(L, "refreshRate");
    if (lua_isnumber(L, -1)) {
        refresh_rate = lua_tointeger(L, -1);
    }
    lua_pop(L, 1); // Remove 'refreshRate' from the stack

    lua_getglobal(L, "mapsCacheCycles");
    if (lua_isnumber(L, -1)) {
        maps_cache_cycles = lua_tointeger(L, -1);
        maps_cache_cycles_value = maps_cache_cycles;
    }
    lua_pop(L, 1); // Remove 'mapsCacheCycles' from the stack

    lua_getglobal(L, "useGameTime");
    if (lua_isboolean(L, -1)) {
        use_game_time = lua_toboolean(L, -1);
    }
    lua_pop(L, 1); // Remove 'useGameTime' from the stack
}

/**
 * The state() LASR function.
 *
 * Executes the code in the state() function of the auto splitter.
 *
 * @param L The Lua State
 */
void state(lua_State* L)
{
    call_va(L, "state", "");
}

/**
 * The update() LASR function.
 *
 * Executes the code in the update() function of the auto splitter.
 *
 * @param L The Lua State
 */
void update(lua_State* L)
{
    call_va(L, "update", "");
}

/**
 * The start() LASR function.
 *
 * Executes the code in the start() function of the auto splitter
 * and stores the whether the run has started.
 *
 * @param L The Lua State
 */
void start(lua_State* L)
{
    bool ret;
    if (call_va(L, "start", ">b", &ret)) {
        atomic_store(&call_start, ret);
        if (ret) {
            atomic_store(&run_started, true);
        }
    }
    lua_pop(L, 1); // Remove the return value from the stack
}

/**
 * The split() LASR function.
 *
 * Executes the code in the split() function of the auto splitter.
 *
 * @param L The Lua State
 */
void split(lua_State* L)
{
    bool ret;
    if (call_va(L, "split", ">b", &ret)) {
        atomic_store(&call_split, ret);
    }
    lua_pop(L, 1); // Remove the return value from the stack
}

/**
 * The is_loading() LASR function.
 *
 * Executes the code in the isLoading() function of the auto splitter,
 * allowing for load time removal.
 *
 * @param L The Lua State
 */
void is_loading(lua_State* L)
{
    bool loading;
    if (call_va(L, "isLoading", ">b", &loading)) {
        if (loading != prev_is_loading) {
            atomic_store(&toggle_loading, true);
            prev_is_loading = !prev_is_loading;
        }
    }
    lua_pop(L, 1); // Remove the return value from the stack
}

/**
 * The reset() LASR function.
 *
 * Executes the code in the reset() function of the auto splitter,
 * resetting the internal state of the timer to a pre-start situation.
 *
 * @param L The Lua State
 */
void reset(lua_State* L)
{
    bool shouldReset;
    if (call_va(L, "reset", ">b", &shouldReset)) {
        if (shouldReset)
            atomic_store(&call_reset, true);
    }
    lua_pop(L, 1); // Remove the return value from the stack
}

/**
 * The gameTime() LASR function.
 *
 * Executes the code in the gameTime() function of the auto splitter,
 * converting the game time found from the game's memory.
 *
 * @param L The Lua State
 */
void gameTime(lua_State* L)
{
    int gameTime;
    if (call_va(L, "gameTime", ">i", &gameTime)) {
        // Convert gameTime from milliseconds to the expected time format and update the timer
        atomic_store(&game_time_value, (long long)gameTime * 1000);
        atomic_store(&update_game_time, true);
    }
    lua_pop(L, 1); // Remove the return value from the stack
}

/**
 * Loads the auto splitter Lua file and executes the auto splitter.
 */
void run_auto_splitter()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    disable_functions(L, disabled_functions);
    push_lasr_functions(L, luac_functions);

    char current_file[PATH_MAX];
    strcpy(current_file, auto_splitter_file);

    // Load the Lua file
    if (luaL_loadfile(L, auto_splitter_file) != LUA_OK) {
        // Error loading the file
        const char* error_msg = lua_tostring(L, -1);
        lua_pop(L, 1); // Remove the error message from the stack
        fprintf(stderr, "Lua syntax error: %s\n", error_msg);
        lua_close(L);
        atomic_store(&auto_splitter_enabled, false);
        return;
    }

    // Execute the Lua file
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        // Error executing the file
        const char* error_msg = lua_tostring(L, -1);
        lua_pop(L, 1); // Remove the error message from the stack
        fprintf(stderr, "Lua runtime error: %s\n", error_msg);
        lua_close(L);
        atomic_store(&auto_splitter_enabled, false);
        return;
    }

    lua_getglobal(L, "state");
    bool state_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'state' from the stack

    lua_getglobal(L, "start");
    bool start_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'start' from the stack

    lua_getglobal(L, "split");
    bool split_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'split' from the stack

    lua_getglobal(L, "isLoading");
    bool is_loading_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'isLoading' from the stack

    lua_getglobal(L, "startup");
    bool startup_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'startup' from the stack

    lua_getglobal(L, "reset");
    bool reset_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'reset' from the stack

    lua_getglobal(L, "update");
    bool update_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'update' from the stack

    lua_getglobal(L, "gameTime");
    bool gameTime_exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove 'gameTime' from the stack

    if (startup_exists) {
        startup(L);
    }

    printf("Refresh rate: %d\n", refresh_rate);
    int rate = 1000000 / refresh_rate;

    while (1) {
        struct timespec clock_start;
        clock_gettime(CLOCK_MONOTONIC, &clock_start);

        if (!atomic_load(&auto_splitter_enabled) || strcmp(current_file, auto_splitter_file) != 0 || !process_exists() || process.pid == 0) {
            break;
        }

        if (state_exists) {
            state(L);
        }

        if (update_exists) {
            update(L);
        }

        if (gameTime_exists && use_game_time && atomic_load(&run_started) && !atomic_load(&run_finished)) {
            gameTime(L);
        }

        if (start_exists && !atomic_load(&run_started) && !atomic_load(&run_finished)) {
            start(L);
        }

        if (split_exists && atomic_load(&run_started)) {
            split(L);
        }

        if (is_loading_exists) {
            is_loading(L);
        }

        if (reset_exists) {
            reset(L);
        }

        // Clear the memory maps cache if needed
        maps_cache_cycles_value--;
        if (maps_cache_cycles_value < 1) {
            maps_clearCache();
            maps_cache_cycles_value = maps_cache_cycles;
            // printf("Cleared maps cache\n");
        }

        struct timespec clock_end;
        clock_gettime(CLOCK_MONOTONIC, &clock_end);
        long long duration = (clock_end.tv_sec - clock_start.tv_sec) * 1000000 + (clock_end.tv_nsec - clock_start.tv_nsec) / 1000;
        // printf("duration: %llu\n", duration);
        if (duration < rate) {
            usleep(rate - duration);
        }
    }

    lua_close(L);
}
