#include "settings.h"
#include "src/gui/app_window.h"
#include "src/logging.h"

#include <glib.h>
#include <lauxlib.h>
#include <limits.h>
#include <strings.h>

static GHashTable* settings;
static GMutex user_settings_mutex;

/**
 * @brief Locks the user settings mutex.
 * Use anytime you are accessing or modifying
 * the user's auto splitter settings from the current
 * `ls_game` object.
 */
void lock_user_settings(void)
{
    g_mutex_lock(&user_settings_mutex);
}

/**
 * @brief Unlocks the user settings mutex.
 * Use after you are done accessing or modifying
 * the user's auto splitter settings from the current
 * `ls_game` object.
 */
void unlock_user_settings(void)
{
    g_mutex_unlock(&user_settings_mutex);
}

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

    for (size_t i = 0; i < setting->config.options_count; ++i) {
        g_free(setting->config.options[i].desc);
    }

    g_free(setting->config.options);
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
    if (!lua_isinteger(L, index)) {
        return SETTING_INVALID;
    }

    lua_Integer type = lua_tointeger(L, index);
    if (type < SETTING_BOOLEAN || type >= SETTING_INVALID) {
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

        case SETTING_OPTION:
        case SETTING_INTEGER:
            {
                if (!lua_isinteger(L, index)) {
                    return "invalid value type set for default integer setting";
                }

                lua_Integer intval = lua_tointeger(L, index);
                if (intval < LONG_MIN || intval > LONG_MAX) {
                    return "invalid value set for default integer setting: out of range";
                }

                val->int_val = (long)intval;
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

        case SETTING_ARRAY:
        case SETTING_OBJECT:
        case SETTING_INVALID:
            return "invalid setting type";
    }

    return NULL;
}

static const char* validate_options(lua_State* L, int index, size_t* count)
{
    *count = 0;

    if (!lua_istable(L, index)) {
        return "options must be an array of tables";
    }

    size_t length = lua_rawlen(L, index);
    if (length == 0) {
        return "options must contain at least one valid entry";
    }

    index = lua_absindex(L, index);
    int top = lua_gettop(L);
    size_t entries = 0;
    const char* error = NULL;
    luaL_checkstack(L, 4, "unable to validate setting options");
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (!lua_isinteger(L, -2) || lua_tointeger(L, -2) < 1 || (lua_Unsigned)lua_tointeger(L, -2) > length) {
            error = "options must be a consecutive array";
            goto validate_options_done;
        }

        if (!lua_istable(L, -1)) {
            error = "each option must be a table";
            goto validate_options_done;
        }

        lua_pushliteral(L, "value");
        lua_rawget(L, -2);
        if (!lua_isinteger(L, -1)) {
            error = "each option must have an integer value";
            goto validate_options_done;
        }

        lua_Integer value = lua_tointeger(L, -1);
        if (value < LONG_MIN || value > LONG_MAX) {
            error = "option value is outside the supported integer range";
            goto validate_options_done;
        }

        lua_pop(L, 1);
        lua_pushliteral(L, "desc");
        lua_rawget(L, -2);
        if (!is_valid_string(L, -1)) {
            error = "each option must have a string desc without NUL bytes";
            goto validate_options_done;
        }

        lua_pop(L, 2);
        ++entries;
    }

    if (entries != length) {
        error = "options must be a consecutive array starting at index 1";
        goto validate_options_done;
    }

    *count = length;

validate_options_done:
    lua_settop(L, top);
    return error;
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

    if (!is_valid_string(L, -1)) {
        return luaL_error(L, "[settings.define] name must be a valid string without any NUL bytes");
    }

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
    size_t options_count = 0;
    int options_index = 0;

    // options must be defined
    if (type == SETTING_OPTION) {
        lua_pushliteral(L, "options");
        lua_rawget(L, 2);
        options_index = lua_gettop(L);
        error = validate_options(L, options_index, &options_count);
        if (error) {
            return luaL_error(L, "[settings.define] %s", error);
        }
    }

    Setting* setting = g_new0(Setting, 1);
    setting->config.key = g_strdup(key);
    setting->config.name = g_strdup(name);
    setting->config.type = type;
    setting->config.default_val = default_val;
    setting->config.desc = g_strdup(desc);
    if (type == SETTING_STRING) {
        setting->config.default_val.string_val = g_strdup(default_val.string_val);
    }

    setting->config.options_count = options_count;
    if (options_count > 0) {
        bool valid_default = false;
        setting->config.options = g_malloc0_n(options_count, sizeof(*setting->config.options));
        for (size_t i = 0; i < options_count; ++i) {
            lua_rawgeti(L, options_index, (lua_Integer)(i + 1));
            lua_pushliteral(L, "value");
            lua_rawget(L, -2);
            setting->config.options[i].value = (long)lua_tointeger(L, -1);
            lua_pop(L, 1);
            lua_pushliteral(L, "desc");
            lua_rawget(L, -2);
            setting->config.options[i].desc = g_strdup(lua_tostring(L, -1));
            lua_pop(L, 2);

            if (setting->config.options[i].value == default_val.int_val) {
                valid_default = true;
            }
        }

        if (!valid_default) {
            free_setting(setting);
            return luaL_error(L, "[settings.define] Invalid default value for setting \"%s\"", key);
        }
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

        case SETTING_OPTION:
        case SETTING_INTEGER:
            lua_pushinteger(L, value->int_val);
            break;

        case SETTING_NUMBER:
            lua_pushnumber(L, value->num_val);
            break;

        case SETTING_STRING:
            lua_pushstring(L, value->string_val);
            break;

        case SETTING_ARRAY:
        case SETTING_OBJECT:
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
    luaL_setfuncs(L, functions, 0);
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
        [SETTING_OPTION] = "SETTING_OPTION",
        [SETTING_ARRAY] = "SETTING_ARRAY",
        [SETTING_OBJECT] = "SETTING_OBJECT",
    };

    for (lua_Integer type = SETTING_BOOLEAN; type < SETTING_INVALID; ++type) {
        lua_pushinteger(L, type);
        lua_setglobal(L, type_names[type]);
    }
}

/**
 * @brief Loads the user's auto splitter settings overrides after
 * settings get defined by the script.
 */
static void lasr_user_settings_load(void)
{
    // nothing to do
    if (!settings) {
        return;
    }

    lock_user_settings();

    size_t count;
    UserSetting** user_settings;
    ls_game_user_settings_get(&user_settings, &count);
    if (count == 0 || user_settings == NULL) {
        goto lasr_user_settings_load_unlock;
    }

    for (size_t i = 0; i < count; ++i) {
        UserSetting* user_setting = user_settings[i];
        if (!g_hash_table_contains(settings, user_setting->key)) {
            continue;
        }

        Setting* setting = g_hash_table_lookup(settings, user_setting->key);
        if (!setting) {
            // this should be impossible
            LOG_WARNF("NULL setting definition found for \"%s\"", user_setting->key);
            continue;
        }

        // allow fixing integer to double conversions
        if (setting->config.type == SETTING_NUMBER && user_setting->type == SETTING_INTEGER) {
            user_setting->type = SETTING_NUMBER;
            double new_val = (double)user_setting->val.int_val;
            user_setting->val.num_val = new_val;
        }

        // fix option type
        if (setting->config.type == SETTING_OPTION && (user_setting->type == SETTING_INTEGER || user_setting->type == SETTING_OPTION)) {
            // validate the value for options
            bool valid = false;
            for (size_t i = 0; i < setting->config.options_count; ++i) {
                if (setting->config.options[i].value == user_setting->val.int_val) {
                    valid = true;
                    break;
                }
            }

            if (!valid) {
                LOG_WARNF("Invalid setting value for \"%s\"", user_setting->key);
                continue;
            }

            user_setting->type = SETTING_OPTION;
        }

        if (user_setting->type != setting->config.type) {
            LOG_WARNF("Invalid setting type for \"%s\"", user_setting->key);
            continue;
        }

        setting->set = true;
        if (setting->config.type == SETTING_STRING) {
            g_free(setting->val.string_val);
            setting->val.string_val = g_strdup(user_setting->val.string_val);
        } else {
            setting->val = user_setting->val;
        }
    }

lasr_user_settings_load_unlock:
    unlock_user_settings();
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
        } else {
            lasr_user_settings_load();
        }
    }

    remove_define(L, function_index);
    lua_remove(L, function_index);
    return status;
}
