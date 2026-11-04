#include "settings.h"
#include "src/gui/app_window.h"
#include "src/logging.h"

#include <glib.h>
#include <lauxlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>

// This could be increased in the future if script authors need more as this consumes very little stack
// But I realistically don't see the need for even a fraction of this amount of nesting.
#define SETTING_MAX_DEPTH 64

/**
 * @brief Holds the path and validation error while reading a definition
 *
 * Ancestor tables are tracked to detect circular references.
 */
typedef struct DefinitionReader {
    GString* path;
    char* error;
    const void* ancestors[SETTING_MAX_DEPTH];
} DefinitionReader;

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

/**
 * @brief Frees a value's contents and clears its storage
 *
 * @param type The value's type
 * @param value The value to clear
 */
void lasr_setting_value_clear(SettingType type, SettingVal* value)
{
    if (!value) {
        return;
    }

    switch (type) {
        case SETTING_STRING:
            g_free(value->string_val);
            break;

        case SETTING_ARRAY:
            for (size_t i = 0; i < value->array_val.count; ++i) {
                lasr_setting_value_clear(value->array_val.values[i].type, value->array_val.values[i].val);
                g_free(value->array_val.values[i].val);
            }

            g_free(value->array_val.values);
            break;

        case SETTING_OBJECT:
            for (size_t i = 0; i < value->object_val.count; ++i) {
                g_free(value->object_val.values[i].key);
                lasr_setting_value_clear(value->object_val.values[i].type, value->object_val.values[i].val);
                g_free(value->object_val.values[i].val);
            }

            g_free(value->object_val.values);
            break;

        case SETTING_BOOLEAN:
        case SETTING_INTEGER:
        case SETTING_NUMBER:
        case SETTING_OPTION:
        case SETTING_INVALID:
            break;
    }

    memset(value, 0, sizeof(*value));
}

/**
 * @brief Makes an independent copy of a value
 *
 * @param type The source value's type
 * @param value The source value to copy
 * @param result The setting to copy the source's value to
 * @return bool true on success, false on failure
 */
bool lasr_setting_value_copy(SettingType type, const SettingVal* value, SettingVal* result)
{
    memset(result, 0, sizeof(*result));
    switch (type) {
        case SETTING_STRING:
            result->string_val = strdup(value->string_val);
            return result->string_val != NULL;

        case SETTING_ARRAY:
            if (value->array_val.count) {
                result->array_val.values = g_try_malloc0_n(value->array_val.count, sizeof(*result->array_val.values));
                if (!result->array_val.values) {
                    return false;
                }
            }

            result->array_val.count = value->array_val.count;
            for (size_t i = 0; i < value->array_val.count; ++i) {
                result->array_val.values[i].type = value->array_val.values[i].type;
                result->array_val.values[i].val = g_try_new0(SettingVal, 1);
                if (!result->array_val.values[i].val || !lasr_setting_value_copy(value->array_val.values[i].type, value->array_val.values[i].val, result->array_val.values[i].val)) {
                    goto lasr_setting_value_copy_failed;
                }
            }

            return true;

        case SETTING_OBJECT:
            if (value->object_val.count) {
                result->object_val.values = g_try_malloc0_n(value->object_val.count, sizeof(*result->object_val.values));
                if (!result->object_val.values) {
                    return false;
                }
            }

            result->object_val.count = value->object_val.count;
            for (size_t i = 0; i < value->object_val.count; ++i) {
                result->object_val.values[i].type = value->object_val.values[i].type;
                result->object_val.values[i].key = strdup(value->object_val.values[i].key);
                result->object_val.values[i].val = g_try_new0(SettingVal, 1);
                if (!result->object_val.values[i].key || !result->object_val.values[i].val || !lasr_setting_value_copy(value->object_val.values[i].type, value->object_val.values[i].val, result->object_val.values[i].val)) {
                    goto lasr_setting_value_copy_failed;
                }
            }

            return true;

        case SETTING_BOOLEAN:
        case SETTING_INTEGER:
        case SETTING_NUMBER:
        case SETTING_OPTION:
            *result = *value;
            return true;
        case SETTING_INVALID:
            break;
    }

lasr_setting_value_copy_failed:
    lasr_setting_value_clear(type, result);
    return false;
}

/**
 * @brief Reads a JSON value and its children
 *
 * @param json The JSON value
 * @param type Receives the inferred type
 * @param result Storage for the parsed value
 * @param depth The current nesting depth
 * @return bool true on success, false on failure
 */
static bool value_from_json(const json_t* json, SettingType* type, SettingVal* result, unsigned int depth)
{
    if (depth >= SETTING_MAX_DEPTH) {
        return false;
    }

    if (json_is_boolean(json)) {
        *type = SETTING_BOOLEAN;
        result->bool_val = json_is_true(json);
    } else if (json_is_integer(json)) {
        json_int_t integer = json_integer_value(json);
        if (integer < LONG_MIN || integer > LONG_MAX) {
            return false;
        }

        *type = SETTING_INTEGER;
        result->int_val = (long)integer;
    } else if (json_is_real(json)) {
        *type = SETTING_NUMBER;
        result->num_val = json_real_value(json);
        return isfinite(result->num_val);
    } else if (json_is_string(json)) {
        if (strlen(json_string_value(json)) != json_string_length(json)) {
            return false;
        }

        *type = SETTING_STRING;
        result->string_val = strdup(json_string_value(json));
        return result->string_val != NULL;
    } else if (json_is_array(json)) {
        *type = SETTING_ARRAY;
        size_t count = json_array_size(json);
        if (count) {
            result->array_val.values = g_try_malloc0_n(count, sizeof(*result->array_val.values));
            if (!result->array_val.values) {
                return false;
            }
        }

        result->array_val.count = count;
        for (size_t i = 0; i < count; ++i) {
            result->array_val.values[i].val = g_try_new0(SettingVal, 1);
            if (!result->array_val.values[i].val || !value_from_json(json_array_get(json, i), &result->array_val.values[i].type, result->array_val.values[i].val, depth + 1)) {
                return false;
            }
        }
    } else if (json_is_object(json)) {
        *type = SETTING_OBJECT;
        size_t count = json_object_size(json);
        if (count) {
            result->object_val.values = g_try_malloc0_n(count, sizeof(*result->object_val.values));
            if (!result->object_val.values) {
                return false;
            }
        }

        result->object_val.count = count;
        const char* key;
        json_t* child;
        size_t i = 0;
        json_object_foreach((json_t*)json, key, child)
        {
            result->object_val.values[i].key = strdup(key);
            result->object_val.values[i].val = g_try_new0(SettingVal, 1);
            if (!result->object_val.values[i].key || !result->object_val.values[i].val || !value_from_json(child, &result->object_val.values[i].type, result->object_val.values[i].val, depth + 1)) {
                return false;
            }

            ++i;
        }
    } else {
        return false;
    }

    return true;
}

/**
 * @brief Converts JSON into a setting value
 *
 * @param json The JSON value
 * @param type Receives the inferred type, or SETTING_INVALID on failure
 * @param result Empty storage for the value
 * @return bool true on success, false on failure
 */
bool lasr_setting_value_from_json(const json_t* json, SettingType* type, SettingVal* result)
{
    memset(result, 0, sizeof(*result));
    *type = SETTING_INVALID;
    if (value_from_json(json, type, result, 0)) {
        return true;
    }

    lasr_setting_value_clear(*type, result);
    *type = SETTING_INVALID;
    return false;
}

json_t* lasr_setting_value_to_json(SettingType type, const SettingVal* value)
{
    json_t* result;
    switch (type) {
        case SETTING_BOOLEAN:
            return json_boolean(value->bool_val);

        case SETTING_OPTION:
        case SETTING_INTEGER:
            return json_integer(value->int_val);

        case SETTING_NUMBER:
            return json_real(value->num_val);

        case SETTING_STRING:
            return json_string(value->string_val);

        case SETTING_ARRAY:
            result = json_array();
            if (!result) {
                return NULL;
            }

            for (size_t i = 0; i < value->array_val.count; ++i) {
                json_t* child = lasr_setting_value_to_json(value->array_val.values[i].type, value->array_val.values[i].val);
                if (!child || json_array_append_new(result, child) != 0) {
                    json_decref(result);
                    return NULL;
                }
            }

            return result;

        case SETTING_OBJECT:
            result = json_object();
            if (!result) {
                return NULL;
            }

            for (size_t i = 0; i < value->object_val.count; ++i) {
                json_t* child = lasr_setting_value_to_json(value->object_val.values[i].type, value->object_val.values[i].val);
                if (!child || json_object_set_new(result, value->object_val.values[i].key, child) != 0) {
                    json_decref(result);
                    return NULL;
                }
            }

            return result;
        case SETTING_INVALID:
            return NULL;
    }

    return NULL;
}

/**
 * @brief Finds an object's field definition by key
 *
 * @param definition The object definition
 * @param key The field key
 * @return const SettingDefinition* The field definition, or NULL if not found
 */
static const SettingDefinition* find_field(const SettingDefinition* definition, const char* key)
{
    for (size_t i = 0; i < definition->fields.count; ++i) {
        if (strcmp(definition->fields.values[i].key, key) == 0) {
            return &definition->fields.values[i];
        }
    }

    return NULL;
}

/**
 * @brief Creates a validation error message with its setting path
 *
 * @param error Receives an error message to free with g_free()
 * @param path The setting's location
 * @param reason The reason validation failed
 * @return bool Always false
 */
static bool invalid_value(char** error, const GString* path, const char* reason)
{
    *error = g_strdup_printf("%s: %s", path->str, reason);
    return false;
}

/**
 * @brief Validates and copies a value using its definition
 *
 * @param definition The setting definition
 * @param type The source value's type
 * @param value The source value to copy
 * @param result The resulting value to copy to
 * @param path The current setting path
 * @param depth The current nesting depth
 * @param error Receives an error message to free with g_free()
 * @return bool true if valid, false otherwise
 */
static bool validate_value(const SettingDefinition* definition, SettingType type, const SettingVal* value, SettingVal* result, GString* path, unsigned int depth, char** error)
{
    if (depth >= SETTING_MAX_DEPTH) {
        return invalid_value(error, path, "setting nesting is too deep");
    }

    if (definition->type == SETTING_NUMBER && type == SETTING_INTEGER) {
        result->num_val = (double)value->int_val;
        return true;
    }

    if (type != definition->type && !(definition->type == SETTING_OPTION && type == SETTING_INTEGER)) {
        return invalid_value(error, path, "value does not match the declared type");
    }

    switch (definition->type) {
        case SETTING_OPTION:
            for (size_t i = 0; i < definition->options_count; ++i) {
                if (definition->options[i].value == value->int_val) {
                    result->int_val = value->int_val;
                    return true;
                }
            }

            return invalid_value(error, path, "value is not a defined option");
        case SETTING_NUMBER:
            if (!isfinite(value->num_val)) {
                return invalid_value(error, path, "number must be finite");
            }

            break;
        case SETTING_OBJECT:
            for (size_t i = 0; i < value->object_val.count; ++i) {
                if (!find_field(definition, value->object_val.values[i].key)) {
                    size_t length = path->len;
                    g_string_append_printf(path, ".%s", value->object_val.values[i].key);
                    bool valid = invalid_value(error, path, "field is not defined");
                    g_string_truncate(path, length);
                    return valid;
                }
            }

            if (definition->fields.count) {
                result->object_val.values = g_try_malloc0_n(definition->fields.count, sizeof(*result->object_val.values));
                if (!result->object_val.values) {
                    goto validate_value_failed;
                }
            }

            result->object_val.count = definition->fields.count;
            for (size_t i = 0; i < definition->fields.count; ++i) {
                const SettingDefinition* field = &definition->fields.values[i];
                result->object_val.values[i].key = strdup(field->key);
                result->object_val.values[i].type = field->type;
                result->object_val.values[i].val = g_try_new0(SettingVal, 1);
                if (!result->object_val.values[i].key || !result->object_val.values[i].val) {
                    goto validate_value_failed;
                }

                const SettingVal* child = NULL;
                SettingType child_type = SETTING_INVALID;
                for (size_t j = 0; j < value->object_val.count; ++j) {
                    if (strcmp(field->key, value->object_val.values[j].key) == 0) {
                        child = value->object_val.values[j].val;
                        child_type = value->object_val.values[j].type;
                        break;
                    }
                }

                size_t length = path->len;
                g_string_append_printf(path, ".%s", field->key);
                bool valid = child ? validate_value(field, child_type, child, result->object_val.values[i].val, path, depth + 1, error) : lasr_setting_value_copy(field->type, &field->default_val, result->object_val.values[i].val);
                if (!valid && !*error) {
                    invalid_value(error, path, "unable to copy field default");
                }

                g_string_truncate(path, length);
                if (!valid) {
                    return false;
                }
            }

            return true;

        case SETTING_ARRAY:
            if (value->array_val.count) {
                result->array_val.values = g_try_malloc0_n(value->array_val.count, sizeof(*result->array_val.values));
                if (!result->array_val.values) {
                    goto validate_value_failed;
                }
            }

            result->array_val.count = value->array_val.count;
            for (size_t i = 0; i < value->array_val.count; ++i) {
                result->array_val.values[i].type = definition->items->type;
                result->array_val.values[i].val = g_try_new0(SettingVal, 1);
                if (!result->array_val.values[i].val) {
                    goto validate_value_failed;
                }

                size_t length = path->len;
                g_string_append_printf(path, "[%zu]", i + 1);
                bool valid = validate_value(definition->items, value->array_val.values[i].type, value->array_val.values[i].val, result->array_val.values[i].val, path, depth + 1, error);
                g_string_truncate(path, length);
                if (!valid) {
                    return false;
                }
            }

            return true;
        case SETTING_BOOLEAN:
        case SETTING_INTEGER:
        case SETTING_STRING:
            break;

        case SETTING_INVALID:
            return invalid_value(error, path, "invalid setting type");
    }

    if (lasr_setting_value_copy(definition->type, value, result)) {
        return true;
    }

validate_value_failed:
    return invalid_value(error, path, "unable to allocate setting value");
}

/**
 * @brief Validates and copies a value, filling missing object fields from defaults
 *
 * @param definition A successfully loaded setting definition
 * @param type The value's type
 * @param value The value to copy
 * @param result Empty storage for the validated value
 * @param error Receives an error to free or NULL on success
 * @return bool true if valid, false otherwise
 */
bool lasr_setting_validate(const SettingDefinition* definition, SettingType type, const SettingVal* value, SettingVal* result, char** error)
{
    memset(result, 0, sizeof(*result));
    *error = NULL;
    GString* path = g_string_new(definition->key ? definition->key : definition->name);
    bool valid = validate_value(definition, type, value, result, path, 0, error);
    g_string_free(path, true);
    if (!valid) {
        lasr_setting_value_clear(definition->type, result);
    }

    return valid;
}

/**
 * @brief Frees a setting definition's metadata, default and children
 *
 * @param definition The definition to clear
 */
static void clear_definition(SettingDefinition* definition)
{
    g_free(definition->key);
    g_free(definition->name);
    g_free(definition->desc);
    lasr_setting_value_clear(definition->type, &definition->default_val);
    for (size_t i = 0; i < definition->options_count; ++i) {
        g_free(definition->options[i].desc);
    }

    g_free(definition->options);
    if (definition->items) {
        clear_definition(definition->items);
        g_free(definition->items);
    }

    for (size_t i = 0; i < definition->fields.count; ++i) {
        clear_definition(&definition->fields.values[i]);
    }

    g_free(definition->fields.values);
}

/**
 * @brief Frees a setting, its definition and its values
 *
 * @param data The setting to free
 */
static void free_setting(gpointer data)
{
    Setting* setting = data;
    lasr_setting_value_clear(setting->config.type, &setting->val);
    clear_definition(&setting->config);
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

/**
 * @brief Validates an OPTION setting's choices
 *
 * @param L The current Lua state
 * @param index The options table's stack index
 * @param count Receives the number of choices, or zero on failure
 * @return const char* An error message, or NULL on success
 */
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
 * @brief Pushes a table member without invoking meta methods
 *
 * @param L The current Lua state
 * @param index The table's stack index
 * @param key The member name
 */
static void raw_field(lua_State* L, int index, const char* key)
{
    index = lua_absindex(L, index);
    lua_pushstring(L, key);
    lua_rawget(L, index);
}
/**
 * @brief Checks for a consecutive Lua array starting at index 1
 *
 * @param L The current Lua state
 * @param index The table's stack index
 * @param path The setting path used in errors
 * @return size_t The array length, or raises a Lua error if invalid
 */

static size_t sequence_size(lua_State* L, int index, const char* path)
{
    index = lua_absindex(L, index);
    luaL_checkstack(L, 3, "unable to inspect setting array");
    size_t length = lua_rawlen(L, index);
    size_t count = 0;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        if (!lua_isinteger(L, -2) || lua_tointeger(L, -2) < 1 || (lua_Unsigned)lua_tointeger(L, -2) > length) {
            luaL_error(L, "%s: expected a consecutive array starting at index 1", path);
        }

        ++count;
        lua_pop(L, 1);
    }

    if (count != length) {
        luaL_error(L, "%s: expected a consecutive array starting at index 1", path);
    }

    return count;
}

/**
 * @brief Copies a Lua default using its setting definition
 *
 * @param L The current Lua state
 * @param index The source value's stack index
 * @param definition The setting definition
 * @param result The result value to copy the setting to
 * @param reader The parsing context
 * @param ancestors Tables on the current recursion path
 * @param depth The current nesting depth
 */
static void read_lua_value(lua_State* L, int index, const SettingDefinition* definition, SettingVal* result, DefinitionReader* reader, const void** ancestors, unsigned int depth)
{
    index = lua_absindex(L, index);
    luaL_checkstack(L, 5, "unable to read setting value");
    if (depth >= SETTING_MAX_DEPTH) {
        luaL_error(L, "%s: setting nesting is too deep", reader->path->str);
    }

    if (definition->type != SETTING_ARRAY && definition->type != SETTING_OBJECT) {
        const char* error = setting_set_val(L, index, definition->type, result);
        if (error) {
            luaL_error(L, "%s: %s", reader->path->str, error);
        }

        if (definition->type == SETTING_STRING) {
            result->string_val = g_strdup(result->string_val);
        }

        return;
    }

    if (!lua_istable(L, index)) {
        luaL_error(L, "%s: expected a table", reader->path->str);
    }

    const void* table = lua_topointer(L, index);
    for (unsigned i = 0; i < depth; ++i) {
        if (ancestors[i] == table) {
            luaL_error(L, "%s: cyclic setting value", reader->path->str);
        }
    }

    ancestors[depth] = table;
    if (definition->type == SETTING_ARRAY) {
        size_t count = sequence_size(L, index, reader->path->str);
        result->array_val.values = g_malloc0_n(count, sizeof(*result->array_val.values));
        result->array_val.count = count;
        for (size_t i = 0; i < count; ++i) {
            result->array_val.values[i].type = definition->items->type;
            result->array_val.values[i].val = g_new0(SettingVal, 1);
            lua_rawgeti(L, index, (lua_Integer)(i + 1));
            size_t length = reader->path->len;
            g_string_append_printf(reader->path, "[%zu]", i + 1);
            read_lua_value(L, -1, definition->items, result->array_val.values[i].val, reader, ancestors, depth + 1);
            g_string_truncate(reader->path, length);
            lua_pop(L, 1);
        }
    } else {
        size_t count = 0;
        lua_pushnil(L);
        while (lua_next(L, index)) {
            if (!is_valid_string(L, -2)) {
                luaL_error(L, "%s: object keys must be strings without NUL bytes", reader->path->str);
            }

            if (!find_field(definition, lua_tostring(L, -2))) {
                luaL_error(L, "%s.%s: undefined field", reader->path->str, lua_tostring(L, -2));
            }

            ++count;
            lua_pop(L, 1);
        }

        result->object_val.values = g_malloc0_n(count, sizeof(*result->object_val.values));
        result->object_val.count = count;
        size_t i = 0;
        lua_pushnil(L);
        while (lua_next(L, index)) {
            const char* key = lua_tostring(L, -2);
            const SettingDefinition* field = find_field(definition, key);
            result->object_val.values[i].key = g_strdup(key);
            result->object_val.values[i].type = field->type;
            result->object_val.values[i].val = g_new0(SettingVal, 1);
            size_t length = reader->path->len;
            g_string_append_printf(reader->path, ".%s", key);
            read_lua_value(L, -1, field, result->object_val.values[i].val, reader, ancestors, depth + 1);
            g_string_truncate(reader->path, length);
            ++i;
            lua_pop(L, 1);
        }
    }
}

/**
 * @brief Loads a setting definition and validates its default
 *
 * @param L The current Lua state
 * @param index The definition table's stack index
 * @param definition The definition to populate
 * @param reader The parsing context
 * @param depth The current nesting depth
 */
static void read_definition(lua_State* L, int index, SettingDefinition* definition, DefinitionReader* reader, unsigned depth)
{
    index = lua_absindex(L, index);
    luaL_checkstack(L, 8, "unable to read setting definition");
    if (depth >= SETTING_MAX_DEPTH) {
        luaL_error(L, "%s: definition nesting is too deep", reader->path->str);
    }

    if (!lua_istable(L, index)) {
        luaL_error(L, "%s: definition must be a table", reader->path->str);
    }

    const void* table = lua_topointer(L, index);
    for (unsigned i = 0; i < depth; ++i) {
        if (reader->ancestors[i] == table) {
            luaL_error(L, "%s: cyclic setting definition", reader->path->str);
        }
    }

    reader->ancestors[depth] = table;
    raw_field(L, index, "name");
    if (!is_valid_string(L, -1) || !*lua_tostring(L, -1)) {
        luaL_error(L, "%s: name must be a nonempty string without NUL bytes", reader->path->str);
    }

    definition->name = g_strdup(lua_tostring(L, -1));
    lua_pop(L, 1);
    raw_field(L, index, "type");
    definition->type = get_setting_type(L, -1);
    if (definition->type == SETTING_INVALID) {
        luaL_error(L, "%s: invalid setting type, use the SETTING_* global constants", reader->path->str);
    }

    lua_pop(L, 1);
    raw_field(L, index, "desc");
    if (!lua_isnil(L, -1) && !is_valid_string(L, -1)) {
        luaL_error(L, "%s: desc must be a string without NUL bytes", reader->path->str);
    }

    definition->desc = g_strdup(lua_tostring(L, -1));
    lua_pop(L, 1);

    if (definition->type == SETTING_OPTION) {
        raw_field(L, index, "options");
        size_t count;
        const char* error = validate_options(L, -1, &count);
        if (error) {
            luaL_error(L, "%s: %s", reader->path->str, error);
        }

        definition->options = g_malloc0_n(count, sizeof(*definition->options));
        definition->options_count = count;
        int options_index = lua_gettop(L);
        for (size_t i = 0; i < count; ++i) {
            lua_rawgeti(L, options_index, (lua_Integer)(i + 1));
            raw_field(L, -1, "value");
            definition->options[i].value = (long)lua_tointeger(L, -1);
            // Ensure this new value is unique.
            for (size_t j = 0; j < i; ++j) {
                if (definition->options[j].value == definition->options[i].value) {
                    luaL_error(L, "%s: option values must be unique", reader->path->str);
                }
            }

            lua_pop(L, 1);
            raw_field(L, -1, "desc");
            definition->options[i].desc = g_strdup(lua_tostring(L, -1));
            lua_pop(L, 2);
        }

        lua_pop(L, 1);
    } else if (definition->type == SETTING_ARRAY) {
        raw_field(L, index, "items");
        definition->items = g_new0(SettingDefinition, 1);
        size_t length = reader->path->len;
        g_string_append(reader->path, ".items");
        read_definition(L, -1, definition->items, reader, depth + 1);
        g_string_truncate(reader->path, length);
        lua_pop(L, 1);
    } else if (definition->type == SETTING_OBJECT) {
        raw_field(L, index, "fields");
        if (!lua_istable(L, -1)) {
            luaL_error(L, "%s: fields must be an array of definitions", reader->path->str);
        }

        size_t count = sequence_size(L, -1, reader->path->str);
        definition->fields.values = g_malloc0_n(count, sizeof(*definition->fields.values));
        definition->fields.count = count;
        int fields_index = lua_gettop(L);
        for (size_t i = 0; i < count; ++i) {
            lua_rawgeti(L, fields_index, (lua_Integer)(i + 1));
            if (!lua_istable(L, -1)) {
                luaL_error(L, "%s: each field must be a definition", reader->path->str);
            }

            raw_field(L, -1, "key");
            if (!is_valid_string(L, -1) || !*lua_tostring(L, -1)) {
                luaL_error(L, "%s: field key must be a nonempty string without NUL bytes", reader->path->str);
            }

            const char* key = lua_tostring(L, -1);
            for (size_t j = 0; j < i; ++j) {
                if (strcmp(definition->fields.values[j].key, key) == 0) {
                    luaL_error(L, "%s: field '%s' is already defined", reader->path->str, key);
                }
            }

            SettingDefinition* field = &definition->fields.values[i];
            field->key = g_strdup(key);
            lua_pop(L, 1);
            size_t length = reader->path->len;
            g_string_append_printf(reader->path, ".%s", field->key);
            read_definition(L, -1, field, reader, depth + 1);
            g_string_truncate(reader->path, length);
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    raw_field(L, index, "default");
    const void* ancestors[SETTING_MAX_DEPTH];
    read_lua_value(L, -1, definition, &definition->default_val, reader, ancestors, 0);
    lua_pop(L, 1);

    SettingVal normalized;
    memset(&normalized, 0, sizeof(normalized));
    if (!validate_value(definition, definition->type, &definition->default_val, &normalized, reader->path, 0, &reader->error)) {
        lasr_setting_value_clear(definition->type, &normalized);
        luaL_error(L, "%s", reader->error);
    }

    lasr_setting_value_clear(definition->type, &definition->default_val);
    definition->default_val = normalized;
}

/**
 * @brief Loads a definition inside a protected Lua call
 *
 * @param L The current Lua state
 * @return int Always 0, or raises a Lua error
 */
static int compile_definition(lua_State* L)
{
    SettingDefinition* definition = lua_touserdata(L, 1);
    DefinitionReader* reader = lua_touserdata(L, 2);
    read_definition(L, 3, definition, reader, 0);
    return 0;
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

    if (!is_valid_string(L, 1) || !*lua_tostring(L, 1)) {
        return luaL_error(L, "[settings.define] key must be a nonempty string without NUL bytes");
    }

    const char* key = lua_tostring(L, 1);
    if (g_hash_table_contains(settings, key)) {
        return luaL_error(L, "[settings.define] setting '%s' is already defined", key);
    }

    luaL_checkstack(L, 4, "unable to load setting definition");
    Setting* setting = g_new0(Setting, 1);
    setting->config.key = g_strdup(key);
    DefinitionReader reader = { .path = g_string_new(key) };
    lua_pushcfunction(L, compile_definition);
    lua_pushlightuserdata(L, &setting->config);
    lua_pushlightuserdata(L, &reader);
    lua_pushvalue(L, 2);
    int status = lua_pcall(L, 3, 0, 0);
    g_string_free(reader.path, true);
    g_free(reader.error);
    if (status != LUA_OK) {
        free_setting(setting);
        return lua_error(L);
    }

    g_hash_table_insert(settings, setting->config.key, setting);
    return 0;
}

/**
 * @brief Pushes an independent Lua copy of a setting value
 *
 * @param L The current Lua state
 * @param type The value's type
 * @param value The value to return
 */
static void push_value(lua_State* L, SettingType type, const SettingVal* value)
{
    luaL_checkstack(L, 3, "unable to return setting value");
    switch (type) {
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
            lua_createtable(L, (int)MIN(value->array_val.count, INT_MAX), 0);
            for (size_t i = 0; i < value->array_val.count; ++i) {
                push_value(L, value->array_val.values[i].type, value->array_val.values[i].val);
                lua_rawseti(L, -2, (lua_Integer)(i + 1));
            }

            break;

        case SETTING_OBJECT:
            lua_createtable(L, 0, (int)MIN(value->object_val.count, INT_MAX));
            for (size_t i = 0; i < value->object_val.count; ++i) {
                lua_pushstring(L, value->object_val.values[i].key);
                push_value(L, value->object_val.values[i].type, value->object_val.values[i].val);
                lua_rawset(L, -3);
            }

            break;

        case SETTING_INVALID:
            luaL_error(L, "[settings.get] invalid setting type");
    }
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
    push_value(L, setting->config.type, value);
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

        SettingVal value = { 0 };
        char* error = NULL;
        if (!lasr_setting_validate(&setting->config, user_setting->type, &user_setting->val, &value, &error)) {
            LOG_WARNF("Invalid setting value: %s", error);
            g_free(error);
            continue;
        }

        lasr_setting_value_clear(setting->config.type, &setting->val);
        setting->val = value;
        setting->set = true;

        // Fix setting types
        if (setting->config.type == SETTING_NUMBER && user_setting->type == SETTING_INTEGER) {
            user_setting->val.num_val = (double)user_setting->val.int_val;
            user_setting->type = SETTING_NUMBER;
        } else if (setting->config.type == SETTING_OPTION) {
            user_setting->type = SETTING_OPTION;
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
