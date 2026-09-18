#pragma once

#include <lua.h>
#include <stdbool.h>

typedef enum SettingType {
    SETTING_BOOLEAN,
    SETTING_INTEGER,
    SETTING_NUMBER,
    SETTING_STRING,
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

void lasr_settings_clear(void);
void lasr_settings_register(lua_State* L);
int lasr_settings_load(lua_State* L);
Setting* lasr_settings_lookup(const char* name);