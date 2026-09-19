#pragma once

#include <lua.h>
#include <stdbool.h>

/**
 * @brief Indicates the type of the value.
 * Since this gets stored in the json file, make sure
 * we explicitly define the values to ensure they never change.
 * Always append to this list, never change values.
 * Make sure new settings go before SETTING_INVALID
 * SETTING_INVALID should always just be the last value so
 * do not define its value.
 */
typedef enum SettingType {
    SETTING_BOOLEAN = 0,
    SETTING_INTEGER = 1,
    SETTING_NUMBER = 2,
    SETTING_STRING = 3,
    SETTING_INVALID, // Easy way to compare a value >= this is invalid. Keep this last
} SettingType;

typedef union SettingVal {
    bool bool_val;
    long int_val;
    double num_val;
    char* string_val;
} SettingVal;

typedef struct SettingDefinition {
    char* key;
    char* name;
    SettingType type;
    SettingVal default_val;
    char* desc;
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