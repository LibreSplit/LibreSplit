/** \file auto-splitter.c
 *
 * Implementation of the auto splitter Lua Runtime
 */
#include "auto-splitter.h"

#include "./maps/maps.h"
#include "functions.h"
#include "game.h"
#include "settings.h"
#include "src/logging.h"
#include "src/timer.h"
#include "utils.h"

#include <errno.h>
#include <glib.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <poll.h>
#include <pthread.h>
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
static GRecMutex lasr_mutex;
static lua_State* event_state; /*!< Borrowed state while the loaded script is active */

/**
 * @brief Loading time uses its own timer to prevent threading/locking issues from reusing the real-time clock.
 */
static pthread_mutex_t loading_mutex = PTHREAD_MUTEX_INITIALIZER;
static long long loading_elapsed;
static long long loading_since;
static bool loading_time_valid;
static GQueue loading_requests = G_QUEUE_INIT;

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
atomic_bool call_split = false; /*!< True if the auto splitter is requesting to split */
atomic_bool call_reset = false; /*!< True if the auto splitter is requesting a run reset */
atomic_bool run_using_game_time_call; /*!< True if startup has run and a new value for using game time has been set by the auto splitter */
atomic_bool run_using_game_time; /*!< True if the auto splitter is requesting to use game time, false for real time */
atomic_bool run_started = false; /*!< Wheter a run was started or not, same as timer->started but accessible from the auto splitter thread */
atomic_bool run_running = false; /*!< Wheter we are running or not, same as timer->running but accessible from the auto splitter thread */
atomic_bool prev_is_loading = false; /*!< The previous frame "is_loading" state */

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
 * @brief Initializes all auto_splitter values to their defaults
 * Call this when opening a new game.
 */
void init_auto_splitter(void)
{
    // don't init auto_splitter_enabled, or auto_splitter_running here
    atomic_store(&call_start, false);
    atomic_store(&call_split, false);
    atomic_store(&call_reset, false);
    atomic_store(&run_using_game_time, false);
    atomic_store(&run_using_game_time_call, false);
    atomic_store(&game_time_value, 0);
    atomic_store(&update_game_time, false);

    pthread_mutex_lock(&loading_mutex);
    g_queue_clear(&loading_requests);
    loading_elapsed = 0;
    loading_since = 0;
    loading_time_valid = false;
    atomic_store(&prev_is_loading, false);
    pthread_mutex_unlock(&loading_mutex);

    // lasr initial values
    use_game_time = false;
    refresh_rate = 60;
    maps_cache_cycles = 1;
    maps_cache_cycles_value = 1;
}

/**
 * Check if the game process exists and is running.
 *
 * @returns Zero if the process is not running, non-zero if it is.
 */
static int process_exists(void)
{
    struct pollfd status = { .fd = process.pidfd, .events = POLLIN };
    int result = poll(&status, 1, 0);
    if (result < 0 && errno != EINTR) {
        LOG_ERRF("Cannot check game process exit: %s", strerror(errno));
    }

    return result <= 0;
}

static int luaopen_os_safe(lua_State* L)
{
    static const char* const allowed_functions[] = { "clock", "date", "difftime", "time", NULL };
    luaopen_os(L);
    lua_newtable(L);
    for (size_t i = 0; allowed_functions[i] != NULL; ++i) {
        lua_getfield(L, -2, allowed_functions[i]);
        lua_setfield(L, -2, allowed_functions[i]);
    }

    lua_remove(L, -2);
    static const luaL_Reg custom_functions[] = {
        { "time_us", time_us },
        { "difftime_us", difftime_us },
        { NULL, NULL }
    };

    luaL_setfuncs(L, custom_functions, 0);
    return 1;
}

/**
 * Lua libraries to enable in LASR
 */
static const luaL_Reg lj_lib_load[] = {
    { LUA_GNAME, luaopen_base },
    { LUA_COLIBNAME, luaopen_coroutine },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_OSLIBNAME, luaopen_os_safe },
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
    { "getMaps", getMaps },
    { "str2ida", str2ida },
    { "md5sum", md5sum },
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

static void push_lasr_globals(lua_State* L)
{
    lua_pushinteger(L, POINTER_SIZE_32);
    lua_setglobal(L, "POINTER_SIZE_32");

    lua_pushinteger(L, POINTER_SIZE_64);
    lua_setglobal(L, "POINTER_SIZE_64");
}

/**
 * Updated version of the library loading routine, made for Lua 5.4
 *
 * @param L The Lua State
 * @param name The library name
 * @param openf The C function to associate to the name
 */
static void open_library(lua_State* L, const char* name, lua_CFunction openf)
{
    luaL_requiref(L, name, openf, 1);
    lua_pop(L, 1);
}

/**
 * A sandboxed variant standard openlibs functions to open only a subset
 * of libraries in the Lua Runtime.
 *
 * @param L The lua Stack
 */
LUALIB_API void sandboxed_openlibs(lua_State* L)
{
    const luaL_Reg* lib;
    for (lib = lj_lib_load; lib->func; lib++) {
        open_library(L, lib->name, lib->func);
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
 * @brief Lua error function for generating a stacktrace.
 *
 * For use as the `errfunc` argument to `lua_pcall()`:
 * ```c
 * lua_pushcfunction(L, traceback);
 * int base = lua_gettop(L);
 * lua_getglobal(L, "func");
 * int status = lua_pcall(L, 0, 1, base);
 * lua_remove(L, base); // remove traceback function
 * if (status != LUA_OK) {
 * 	// ...
 * }
 * ```
 *
 * @param L Takes a single error object from the top of the Lua stack.
 * @return Pushes a single string containing a formatted stacktrace.
 * @copyright MIT license; borrowed from the LuaJIT interpreter.
 */
static int traceback(lua_State* L)
{
    const char* message = lua_tostring(L, 1);
    if (!message) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_isstring(L, -1)) {
            message = lua_tostring(L, -1);
        } else {
            message = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
    }

    luaL_traceback(L, L, message, 1);
    return 1;
}

/**
 * @brief Modify Lua stacktrace to contain a readable name for the top-level function.
 *
 * When calling global Lua functions from C, `luaL_traceback()` loses track of the function name
 * because it is not associated with any bytecode slot. This results in tracebacks that end in
 * `in function </path/to/script.lua:line>` rather than `in function 'update'`. This procedure
 * substitutes that part for an explicit function name, replacing the top of the stack with the
 * modified string.
 *
 * @param L Expects a single error string to be at the top of the stack, such as is produced by `luaL_traceback()`.
 * @param func The name of the function called from C code.
 */
static void pcall_fix_traceback(lua_State* L, const char* func)
{
    if (!lua_isstring(L, -1)) {
        return;
    }

    const char* trace = lua_tostring(L, -1);
    const char* last_line = strrchr(trace, '\n');
    if (last_line == NULL) {
        LOG_WARN("lua traceback: invalid trace, traceback should have at least one new line");
        return;
    }

    // "\t/path/to/script.lua:line: in function </path/to/script.lua:line>"
    if (strlen(last_line) <= strlen(auto_splitter_file) + 1) {
        // the trace should include the path and therefore be bigger.
        return;
    }

    const char* path = strchr(last_line + 1 + strlen(auto_splitter_file), '<'); // auto splitter path may contain a `<` character
    if (path == NULL) {
        return;
    }

    lua_pushlstring(L, trace, path - trace);
    lua_pushfstring(L, "'%s'", func);
    lua_concat(L, 2);
    lua_remove(L, -2); // remove original stacktrace string
}

/**
    Generic function to call lua functions
    Signatures are something like `disb>s`
    1. d = double
    2. i = int
    3. s = string
    4. b = boolean
    5. > = return separator

    Example: `call_va(L, "functionName", "dd>d", x, y, &z);`
    Calls "functionName" with two doubles as parameters (x and y), returning a double in z

    Restores the entry stack height on success and failure; callers must not pop results.
    String results are borrowed from Lua and must be copied before further Lua calls.
*/
bool call_va(lua_State* L, const char* func, const char* sig, ...)
{
    va_list vl;
    int narg, nres; /* number of arguments and results */
    int status, base;
    int entry_top = lua_gettop(L);
    bool success = false;

    va_start(vl, sig);
    lua_pushcfunction(L, traceback);
    base = lua_gettop(L);
    lua_getglobal(L, func); /* get function */

    /* push arguments */
    narg = 0;
    while (*sig) { /* push arguments */
        switch (*sig++) {
            case 'd': /* double argument */
                lua_pushnumber(L, va_arg(vl, double));
                break;

            case 'i': /* int argument */
                lua_pushinteger(L, va_arg(vl, lua_Integer));
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
                goto call_va_cleanup;
        }
        if (*(sig - 1) == '>')
            break;
        narg++;
        luaL_checkstack(L, 1, "too many arguments");
    }

    /* do the call */
    nres = strlen(sig); /* number of expected results */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base); /* remove traceback function */
    if (status != LUA_OK) {
        pcall_fix_traceback(L, func);
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "Lua %s error: %s\n", func, err ? err : "(error object is not a string)");
        goto call_va_cleanup;
    }

    if (nres == 0) {
        success = true;
        goto call_va_cleanup;
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
                        goto call_va_cleanup;
                    }
                    *va_arg(vl, double*) = lua_tonumber(L, nres);
                    break;

                case 'i': /* int result */
                    if (!lua_isinteger(L, nres)) {
                        printf("function '%s' wrong result type, expected int\n", func);
                        goto call_va_cleanup;
                    }
                    *va_arg(vl, lua_Integer*) = lua_tointeger(L, nres);
                    break;

                case 's': /* string result */
                    if (!lua_isstring(L, nres)) {
                        printf("function '%s' wrong result type, expected string\n", func);
                        goto call_va_cleanup;
                    }
                    *va_arg(vl, const char**) = lua_tostring(L, nres);
                    break;

                case 'b':
                    if (!lua_isboolean(L, nres)) {
                        printf("function '%s' wrong result type, expected boolean\n", func);
                        goto call_va_cleanup;
                    }
                    *va_arg(vl, bool*) = lua_toboolean(L, nres);
                    break;

                default:
                    printf("invalid option (%c)\n", *(sig - 1));
                    goto call_va_cleanup;
            }
            nres++;
        }
    } else {
        goto call_va_cleanup;
    }

    success = true;

call_va_cleanup:
    lua_settop(L, entry_top);
    va_end(vl);
    return success;
}

/**
 * The startup() LASR function.
 *
 * Executes the code in the startup() function of the auto splitter,
 * setting the internal parameters for the execution of the autosplitter.
 *
 * @param L The Lua State.
 * @return bool Whether or not the startup operation was successful.
 */
bool startup(lua_State* L)
{
    if (!atomic_load(&auto_splitter_enabled) || !call_va(L, "startup", "")) {
        return false;
    }

    lua_getglobal(L, "refreshRate");
    if (lua_isinteger(L, -1)) {
        lua_Integer val = lua_tointeger(L, -1);
        if (val < 1 || val > INT32_MAX) {
            fprintf(stderr, "Lua startup error: 'refreshRate' can not be less than 1 or greater than %i\n", INT32_MAX);
            return false;
        }

        refresh_rate = val;
    } else if (!lua_isnil(L, -1)) {
        fprintf(stderr, "Lua startup error: 'refreshRate' must be an integer\n");
        return false;
    }

    lua_pop(L, 1); // Remove 'refreshRate' from the stack

    lua_getglobal(L, "mapsCacheCycles");
    if (lua_isinteger(L, -1)) {
        lua_Integer val = lua_tointeger(L, -1);
        if (val < 0 || val > INT32_MAX) {
            fprintf(stderr, "Lua startup error: 'mapsCacheCycles' can not be less than 0 or greater than %i\n", INT32_MAX);
            return false;
        }

        maps_cache_cycles = val;
        maps_cache_cycles_value = maps_cache_cycles;
    } else if (!lua_isnil(L, -1)) {
        fprintf(stderr, "Lua startup error: 'mapsCacheCycle' must be an integer\n");
        return false;
    }

    lua_pop(L, 1); // Remove 'mapsCacheCycles' from the stack

    lua_getglobal(L, "useGameTime");
    if (lua_isboolean(L, -1)) {
        use_game_time = lua_toboolean(L, -1);
        atomic_store(&run_using_game_time, use_game_time);
        atomic_store(&run_using_game_time_call, true);
    } else if (!lua_isnil(L, -1)) {
        fprintf(stderr, "Lua startup error: 'useGameTime' must be a boolean\n");
        return false;
    } else {
        atomic_store(&run_using_game_time, false); // Default to real time if not specified
        atomic_store(&run_using_game_time_call, true);
    }

    lua_pop(L, 1); // Remove 'useGameTime' from the stack
    return true;
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
        if (ret) {
            atomic_store(&call_start, true);
        }
    }
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
        pthread_mutex_lock(&loading_mutex);
        if (!atomic_load(&auto_splitter_enabled)) {
            pthread_mutex_unlock(&loading_mutex);
            return;
        }

        bool was_loading = atomic_load(&prev_is_loading);
        if (!loading_time_valid || loading != was_loading) {
            long long now = ls_time_now();
            if (was_loading) {
                loading_elapsed += now - loading_since;
            }

            loading_since = now;
            loading_time_valid = true;
            atomic_store(&prev_is_loading, loading);
            g_queue_push_tail(&loading_requests, GINT_TO_POINTER(loading));
        }

        pthread_mutex_unlock(&loading_mutex);
    }
}

/**
 * @brief Return accumulated loading time, including any current loading interval.
 *
 * @param[out] now Receives the monotonic timestamp of this reading, in microseconds.
 * @return Total loading time in microseconds since init_auto_splitter().
 * @note Uses a separate lock and does not wait for Lua execution.
 */
long long lasr_loading_time(long long* now)
{
    pthread_mutex_lock(&loading_mutex);
    *now = ls_time_now();
    long long elapsed = loading_elapsed;
    if (atomic_load(&prev_is_loading)) {
        elapsed += *now - loading_since;
    }

    pthread_mutex_unlock(&loading_mutex);
    return elapsed;
}

/**
 * @brief Check whether the timer has a loading request to process.
 * @return true if a loading request is queued, otherwise false.
 */
bool lasr_has_loading_request(void)
{
    pthread_mutex_lock(&loading_mutex);
    bool pending = !g_queue_is_empty(&loading_requests);
    pthread_mutex_unlock(&loading_mutex);
    return pending;
}

/**
 * @brief Remove and return the oldest pending loading request.
 * @pre Hold the Lua lock.
 * @return 1 to enter loading, 0 to leave loading, or -1 if none is pending.
 */
int lasr_loading_request_pop(void)
{
    pthread_mutex_lock(&loading_mutex);
    int loading = !g_queue_is_empty(&loading_requests) ? GPOINTER_TO_INT(g_queue_pop_head(&loading_requests)) : -1;
    pthread_mutex_unlock(&loading_mutex);
    return loading;
}

static void clear_loading_requests(void)
{
    pthread_mutex_lock(&loading_mutex);
    g_queue_clear(&loading_requests);
    loading_time_valid = false;
    pthread_mutex_unlock(&loading_mutex);
}

/**
 * @brief Clear pending loading requests when resetting an attempt.
 *
 * If LASR is disabled, also stop measuring the current loading interval.
 * @pre Hold the Lua lock, or stop LASR before calling.
 * @return Latest sampled loading state, or false if LASR is disabled.
 */
bool lasr_reset_loading(void)
{
    pthread_mutex_lock(&loading_mutex);
    bool loading = atomic_load(&prev_is_loading);
    if (!atomic_load(&auto_splitter_enabled)) {
        if (loading) {
            loading_elapsed += ls_time_now() - loading_since;
        }

        loading = false;
        atomic_store(&prev_is_loading, false);
        loading_time_valid = false;
    }

    g_queue_clear(&loading_requests);
    pthread_mutex_unlock(&loading_mutex);
    return loading;
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
        if (shouldReset) {
            atomic_store(&call_reset, true);
        }
    }
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
    lua_Integer gameTime;
    if (call_va(L, "gameTime", ">i", &gameTime)) {
        if (gameTime < INT64_MIN / 1000 || gameTime > INT64_MAX / 1000) {
            fprintf(stderr, "Lua gameTime eror: result is outside the supported millisecond range\n");
            return;
        }

        // Convert gameTime from milliseconds to the expected time format and update the timer
        atomic_store(&game_time_value, (long long)gameTime * 1000LL);
        atomic_store(&update_game_time, true);
    }
}

/**
 * Utility function to check if a Lua function is defined in an auto splitter
 *
 * @param L The lua state
 * @param name The function name to look for
 *
 * @returns True if the function is defined in the auto splitter, false otherwise
 */
static bool has_lua_function(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    bool exists = lua_isfunction(L, -1);
    lua_pop(L, 1); // Remove function from the stack
    return exists;
}

void lasr_lock(void)
{
    g_rec_mutex_lock(&lasr_mutex);
}

void lasr_unlock(void)
{
    g_rec_mutex_unlock(&lasr_mutex);
}

/**
 * @brief Call a Lua event hook and an optional second hook synchronously.
 *
 * Skip undefined hooks. Do nothing if LASR is disabled or no script is active.
 * @pre Hold the Lua lock while changing the timer and calling this function.
 * @param name Name of the first Lua event hook.
 * @param following Name of the next hook, or NULL.
 */
void lasr_event(const char* name, const char* following)
{
    if (event_state && atomic_load(&auto_splitter_enabled)) {
        if (has_lua_function(event_state, name)) {
            call_va(event_state, name, "");
        }

        if (following && has_lua_function(event_state, following)) {
            call_va(event_state, following, "");
        }
    }
}

/**
 * Loads the auto splitter Lua file and executes the auto splitter.
 */
void run_auto_splitter(void)
{
    LOG_DEBUG("Starting new Lua State")
    lua_State* L = luaL_newstate();
    sandboxed_openlibs(L);
    disable_functions(L, disabled_functions);
    push_lasr_functions(L, luac_functions);
    push_lasr_globals(L);
    lasr_game_register(L);
    lasr_settings_register(L);

    char current_file[PATH_MAX];
    strcpy(current_file, auto_splitter_file);

    // Load the Lua file
    lua_pushcfunction(L, traceback);
    int base = lua_gettop(L);
    if (luaL_loadfile(L, auto_splitter_file) != LUA_OK) {
        // Error loading the file
        const char* error_msg = lua_tostring(L, -1);
        fprintf(stderr, "Lua syntax error: %s\n", error_msg);
        lua_pop(L, 1); // Remove the error message from the stack
        lua_close(L);
        clear_process();
        maps_clearCache();
        lasr_settings_clear();
        atomic_store(&auto_splitter_enabled, false);
        return;
    }

    // Execute the Lua file
    set_process_selection(true);
    int status = lua_pcall(L, 0, LUA_MULTRET, base);
    set_process_selection(false);
    if (status != LUA_OK || lasr_settings_load(L) != LUA_OK) {
        // Error executing the file
        if (!lua_isnil(L, -1)) {
            const char* err = lua_tostring(L, -1);
            if (err == NULL)
                err = "(error object is not a string)";
            fprintf(stderr, "%s\n", err);
        }

        lua_pop(L, 1);
        lua_close(L);
        clear_process();
        maps_clearCache();
        lasr_settings_clear();
        atomic_store(&auto_splitter_enabled, false);
        return;
    }

    lua_remove(L, base); /* remove traceback function */

    bool state_exists = has_lua_function(L, "state");
    bool start_exists = has_lua_function(L, "start");
    bool split_exists = has_lua_function(L, "split");
    bool is_loading_exists = has_lua_function(L, "isLoading");
    bool startup_exists = has_lua_function(L, "startup");
    bool init_exists = has_lua_function(L, "init");
    bool exit_exists = has_lua_function(L, "exit");
    bool reset_exists = has_lua_function(L, "reset");
    bool update_exists = has_lua_function(L, "update");
    bool gameTime_exists = has_lua_function(L, "gameTime");

    if (startup_exists && !startup(L)) {
        atomic_store(&auto_splitter_enabled, false);
        goto run_auto_splitter_cleanup;
    }

    printf("Refresh rate: %d\n", refresh_rate);
    int rate = 1000000 / refresh_rate;
    bool initialized = false;

    lasr_lock();
    if (atomic_load(&auto_splitter_enabled)) {
        event_state = L;
    }

    lasr_unlock();

    while (1) {
        struct timespec clock_start;
        clock_gettime(CLOCK_MONOTONIC, &clock_start);

        lasr_lock();
        if (!atomic_load(&auto_splitter_enabled) || strcmp(current_file, auto_splitter_file) != 0) {
            lasr_unlock();
            break;
        }

        int delay = rate;
        if (process.pid && !process_exists()) {
            detach_process();
            maps_cache_cycles_value = maps_cache_cycles;
            initialized = false;
            if (exit_exists) {
                call_va(L, "exit", "");
            }
        } else if (!process.pid) {
            delay = 100000;
            lasr_unlock();
            bool attached = attach_process();
            lasr_lock();
            if (!atomic_load(&auto_splitter_enabled) || strcmp(current_file, auto_splitter_file) != 0) {
                lasr_unlock();
                break;
            }

            if (attached) {
                maps_cache_cycles_value = maps_cache_cycles;
                initialized = !init_exists || call_va(L, "init", "");
            }
        } else if (!initialized) {
            initialized = !init_exists || call_va(L, "init", "");
        } else {
            if (state_exists) {
                state(L);
            }

            if (update_exists) {
                update(L);
            }

            if (gameTime_exists && use_game_time && atomic_load(&run_started) && atomic_load(&run_running)) {
                gameTime(L);
            }

            if (start_exists && !atomic_load(&run_started) && !atomic_load(&run_running) && !atomic_load(&call_start) && !atomic_load(&call_reset)) {
                start(L);
            }

            if (split_exists && atomic_load(&run_started) && !atomic_load(&call_split) && !atomic_load(&call_reset)) {
                split(L);
            }

            if (is_loading_exists) {
                is_loading(L);
            }

            if (reset_exists && atomic_load(&run_running) && !atomic_load(&call_reset)) {
                reset(L);
            }
        }

        // Clear the memory maps cache if needed
        maps_cache_cycles_value--;
        if (maps_cache_cycles_value < 1) {
            maps_clearCache();
            maps_cache_cycles_value = maps_cache_cycles;
            // printf("Cleared maps cache\n");
        }

        lasr_unlock();

        struct timespec clock_end;
        clock_gettime(CLOCK_MONOTONIC, &clock_end);
        long long duration = (clock_end.tv_sec - clock_start.tv_sec) * 1000000 + (clock_end.tv_nsec - clock_start.tv_nsec) / 1000;
        // printf("duration: %llu\n", duration);
        if (duration < delay) {
            usleep(delay - duration);
        }
    }

run_auto_splitter_cleanup:
    lasr_lock();
    event_state = NULL;
    clear_loading_requests();
    atomic_store(&call_start, false);
    atomic_store(&call_split, false);
    atomic_store(&call_reset, false);
    if (has_lua_function(L, "shutdown")) {
        call_va(L, "shutdown", "");
    }

    lua_close(L);
    clear_process();
    maps_clearCache();
    lasr_settings_clear();
    lasr_unlock();
}

/**
 * @brief Stops the auto splitter and waits for the process to end.
 * The wait is very quick so doing it on another thread should be fine.
 *
 * If auto splitter wasn't running, does nothing
 */
void stop_auto_splitter(void)
{
    atomic_store(&auto_splitter_enabled, false);

    // Make sure any current transitions finish before stopping.
    lasr_lock();
    event_state = NULL;
    clear_loading_requests();
    lasr_unlock();

    while (atomic_load(&auto_splitter_running)) {
        // wait, this will be very fast so its ok to just spin
    }
}
