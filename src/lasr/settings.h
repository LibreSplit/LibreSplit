#pragma once

#include <jansson.h>
#include <lua.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Indicates the type of the value.
 * Always append to this list, never change values.
 * Make sure new settings go before SETTING_INVALID
 * SETTING_INVALID should always just be the last value.
 */
typedef enum SettingType {
    SETTING_BOOLEAN,
    SETTING_INTEGER,
    SETTING_NUMBER,
    SETTING_STRING,
    SETTING_OPTION,
    SETTING_ARRAY,
    SETTING_OBJECT,
    SETTING_INVALID, // Easy way to compare a value >= this is invalid. Keep this last
} SettingType;

typedef union SettingVal {
    bool bool_val;
    long int_val;
    double num_val;
    char* string_val;
    struct {
        struct {
            SettingType type;
            union SettingVal* val;
        }* values;
        size_t count;
    } array_val;
    struct {
        struct {
            char* key;
            SettingType type;
            union SettingVal* val;
        }* values;
        size_t count;
    } object_val;
} SettingVal;

typedef struct SettingDefinition {
    char* key;
    char* name;
    SettingType type;
    SettingVal default_val;
    char* desc;
    struct {
        long value;
        char* desc;
    }* options;
    size_t options_count;
    struct SettingDefinition* items;
    struct {
        struct SettingDefinition* values;
        size_t count;
    } fields;
} SettingDefinition;

typedef struct Setting {
    SettingDefinition config;
    SettingVal val;
    bool set;
} Setting;

typedef struct UserSetting {
    char* key;
    SettingVal val;
    SettingType type;
} UserSetting;

void lock_user_settings(void);
void unlock_user_settings(void);
void lasr_settings_clear(void);
void lasr_settings_register(lua_State* L);
int lasr_settings_load(lua_State* L);
Setting* lasr_settings_lookup(const char* name);
void lasr_setting_value_clear(SettingType type, SettingVal* value);
bool lasr_setting_value_copy(SettingType type, const SettingVal* value, SettingVal* result);
bool lasr_setting_value_from_json(const json_t* json, SettingType* type, SettingVal* result);
json_t* lasr_setting_value_to_json(SettingType type, const SettingVal* value);
bool lasr_setting_validate(const SettingDefinition* definition, SettingType type, const SettingVal* value, SettingVal* result, char** error);
