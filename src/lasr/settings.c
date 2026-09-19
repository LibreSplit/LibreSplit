#include "settings.h"
#include "src/logging.h"

#include <glib.h>
#include <lauxlib.h>
#include <limits.h>
#include <math.h>
#include <strings.h>

static GHashTable* settings;

static void free_setting(gpointer data)
{
    Setting* setting = data;
    g_free(setting->config.key);
    g_free(setting->config.name);
    g_free(setting->config.desc);
    if (setting->config.type == SETTING_STRING) {
        g_free(setting->config.default_val.string_val);
        g_free(setting->val.string_val);
    }

    g_free(setting);
}

/**
 * @brief Clears the settings for the currently loaded autosplitter
 */
void lasr_settings_clear(void)
{
    g_clear_pointer(&settings, g_hash_table_destroy);
}

/**
 * @brief Makes sure the parameters at index is a valid lua string
 * and contains no null characters embedded in the string.
 *
 * @param L The current Lua state.
 * @param index The param index in Lua.
 * @return bool Whether or not the string is a valid Lua string without any null characters.
 */
static bool is_valid_string(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TSTRING) {
        return false;
    }

    size_t length;
    const char* value = lua_tolstring(L, index, &length);
    return memchr(value, '\0', length) == NULL;
}

/**
 * @brief Get the setting type of the setting.
 *
 * @param L The current Lua state.
 * @param index The index of the setting param.
 * @return SettingType The type of the setting supplied.
 */
static SettingType get_setting_type(lua_State* L, int index)
{
    if (lua_type(L, index) != LUA_TNUMBER) {
        return SETTING_INVALID;
    }

    lua_Number val = lua_tonumber(L, index);
    lua_Integer type = lua_tointeger(L, index);
    if (val != (lua_Number)type || type < SETTING_BOOLEAN || type >= SETTING_INVALID) {
        return SETTING_INVALID;
    }

    return (SettingType)type;
}

/**
 * @brief Validates the value of a setting and sets the value to the pointer provided at val.
 * val will not be set if the value is invalid for the type.
 *
 * @param L The current Lua state.
 * @param index The parameter index.
 * @param type The type of the setting.
 * @param val Pointer to the setting value to store the value at.
 * @return const char* An error message if the value was invalid or NULL otherwise.
 */
static const char* setting_set_val(lua_State* L, int index, SettingType type, SettingVal* val)
{
    switch (type) {
        case SETTING_BOOLEAN:
            if (!lua_isboolean(L, index)) {
                return "invalid value type set for default boolean setting";
            }

            val->bool_val = lua_toboolean(L, index);
            break;

        case SETTING_INTEGER:
            {
                if (lua_type(L, index) != LUA_TNUMBER) {
                    return "invalid value type set for default integer setting";
                }

                lua_Number num = lua_tonumber(L, index);
                if (!isfinite(num) || (long double)num < LONG_MIN || (long double)num > LONG_MAX) {
                    return "invalid value set for default integer setting: out of range";
                }

                long intval = (long)num;
                if (num != (lua_Number)intval) {
                    return "invalid value set for default integer setting: not an integer";
                }

                val->int_val = intval;
                break;
            }

        case SETTING_NUMBER:
            if (lua_type(L, index) != LUA_TNUMBER) {
                return "invalid value type set for default number setting";
            }

            val->num_val = lua_tonumber(L, index);
            break;

        case SETTING_STRING:
            if (!is_valid_string(L, index)) {
                return "invalid value type set for default string setting";
            }

            val->string_val = (char*)lua_tostring(L, index);
            break;

        case SETTING_INVALID:
            return "invalid setting type";
    }

    return NULL;
}

/**
 * @brief Defines a new setting in the settings global.
 * Validates the script's input and creates a new setting in memory
 * then inserts it to the hash table at "key"
 *
 * @param L The current lua state.
 * @return int Always 0 or error.
 */
static int define_setting(lua_State* L)
{
    if (lua_gettop(L) != 2) {
        return luaL_error(L, "[settings.define] Two arguments are required.");
    }

    if (lua_isnil(L, lua_upvalueindex(1))) {
        return luaL_error(L, "[settings.define] settings.define is only available to be called in `define_settings`");
    }

    if (!settings) {
        return luaL_error(L, "[settings.define] settings was not properly initialized");
    }

    if (!is_valid_string(L, 1)) {
        return luaL_error(L, "[settings.define] key must be a valid string without any NUL bytes");
    }

    if (!lua_istable(L, 2)) {
        return luaL_error(L, "[settings.define] definition must be a table");
    }

    const char* key = lua_tostring(L, 1);
    if (!*key) {
        return luaL_error(L, "[settings.define] key must not be empty");
    }

    if (g_hash_table_contains(settings, key)) {
        return luaL_error(L, "[settings.define] setting \"%s\" is already defined", key);
    }

    lua_pushliteral(L, "name");
    lua_rawget(L, 2);

    const char* name = lua_tostring(L, -1);
    if (!*name) {
        return luaL_error(L, "[settings.define] name must not be empty");
    }

    lua_pop(L, 1);
    lua_pushliteral(L, "type");
    lua_rawget(L, 2);

    SettingType type = get_setting_type(L, -1);
    if (type == SETTING_INVALID) {
        return luaL_error(L, "[settings.define] invalid setting type, use the SETTING_* global constants");
    }

    lua_pop(L, 1);
    lua_pushliteral(L, "default");
    lua_rawget(L, 2);

    SettingVal default_val = { 0 };
    const char* error = setting_set_val(L, -1, type, &default_val);
    if (error) {
        return luaL_error(L, "[settings.define] %s", error);
    }

    lua_pushliteral(L, "desc");
    lua_rawget(L, 2);

    if (!lua_isnil(L, -1) && !is_valid_string(L, -1)) {
        return luaL_error(L, "[settings.define] the settings description must be a valid string without NUL bytes");
    }

    const char* desc = lua_tostring(L, -1);
    Setting* setting = g_new0(Setting, 1);
    setting->config.key = g_strdup(key);
    setting->config.name = g_strdup(name);
    setting->config.type = type;
    setting->config.default_val = default_val;
    setting->config.desc = g_strdup(desc);
    if (type == SETTING_STRING) {
        setting->config.default_val.string_val = g_strdup(default_val.string_val);
    }

    g_hash_table_insert(settings, setting->config.key, setting);
    return 0;
}

/**
 * @brief Gets a setting by "key" or NULL if no settings defined or
 * the value does not exist.
 *
 * @param key The key of the setting.
 * @return Setting* Pointer to the setting or NULL.
 */
Setting* lasr_settings_lookup(const char* key)
{
    return settings && key ? g_hash_table_lookup(settings, key) : NULL;
}

/**
 * @brief Get the value for "key" from settings.
 *
 * @param L The current lua state.
 * @return int Always 1 or error.
 */
static int get_setting(lua_State* L)
{
    if (lua_gettop(L) != 1) {
        return luaL_error(L, "[settings.get] One argument is required.");
    }

    if (!is_valid_string(L, 1)) {
        return luaL_error(L, "[settings.get] key must be a valid string without any NUL bytes");
    }

    const char* key = lua_tostring(L, 1);
    const Setting* setting = lasr_settings_lookup(key);
    if (!setting) {
        lua_pushnil(L);
        return 1;
    }

    const SettingVal* value = setting->set ? &setting->val : &setting->config.default_val;
    switch (setting->config.type) {
        case SETTING_BOOLEAN:
            lua_pushboolean(L, value->bool_val);
            break;

        case SETTING_INTEGER:
            lua_pushinteger(L, value->int_val);
            break;

        case SETTING_NUMBER:
            lua_pushnumber(L, value->num_val);
            break;

        case SETTING_STRING:
            lua_pushstring(L, value->string_val);
            break;

        case SETTING_INVALID:
            // given earlier validation, this should be impossible
            LOG_WARN("Invalid setting type found in settings");
            return luaL_error(L, "[settings.get] invalid setting type");
    }

    return 1;
}

/**
 * @brief After init, calling settings.define should not be allowed.
 * This removes define from the settings global.
 *
 * @param L The current lua state.
 * @param function_index The function index for define in settings.
 */
static void remove_define(lua_State* L, int function_index)
{
    lua_getupvalue(L, function_index, 1);
    lua_pushnil(L);
    lua_setupvalue(L, function_index, 1);

    if (lua_istable(L, -1)) {
        lua_pushliteral(L, "define");
        lua_pushnil(L);
        lua_rawset(L, -3);
    }

    lua_pop(L, 1);
}

/**
 * @brief Registers the global settings object and
 * type definitions that mirror the SettingType enum.
 *
 * @param L The current lua state.
 */
void lasr_settings_register(lua_State* L)
{
    static const luaL_Reg functions[] = {
        { "get", get_setting },
        { NULL, NULL },
    };

    lua_newtable(L);
    luaL_register(L, NULL, functions);
    if (!settings) {
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, define_setting, 1);
        lua_setfield(L, -2, "define");
    }

    lua_setglobal(L, "settings");

    static const char* const type_names[] = {
        [SETTING_BOOLEAN] = "SETTING_BOOLEAN",
        [SETTING_INTEGER] = "SETTING_INTEGER",
        [SETTING_NUMBER] = "SETTING_NUMBER",
        [SETTING_STRING] = "SETTING_STRING",
    };

    for (lua_Integer type = SETTING_BOOLEAN; type < SETTING_INVALID; ++type) {
        lua_pushinteger(L, type);
        lua_setglobal(L, type_names[type]);
    }
}

/**
 * @brief Load settings and define them for Lua.
 * Runs define_settings if it exists and creates the
 * settings hash table.
 *
 * @param L The current lua state.
 * @return int 0 on success otherwise error #
 */
int lasr_settings_load(lua_State* L)
{
    if (settings) {
        return LUA_OK;
    }

    lua_getglobal(L, "settings");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushliteral(L, "[settings] settings must be registered before loading definitions");
        return LUA_ERRRUN;
    }

    lua_pushliteral(L, "define");
    lua_rawget(L, -2);
    lua_remove(L, -2);
    if (lua_tocfunction(L, -1) != define_setting) {
        lua_pop(L, 1);
        lua_pushliteral(L, "[settings] settings.define is unavailable");
        return LUA_ERRRUN;
    }

    int function_index = lua_gettop(L);
    lua_getglobal(L, "define_settings");
    int status = LUA_OK;
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
    } else if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        lua_pushliteral(L, "[settings] define_settings must be a function");
        status = LUA_ERRRUN;
    } else {
        settings = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_setting);
        status = lua_pcall(L, 0, 0, 0);
        if (status != LUA_OK) {
            lasr_settings_clear();
        }
    }

    remove_define(L, function_index);
    lua_remove(L, function_index);
    return status;
}
