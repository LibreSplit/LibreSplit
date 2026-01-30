#pragma once

#include <linux/limits.h>
#include <stddef.h>

typedef enum ConfigType {
    CFG_BOOL,
    CFG_INT,
    CFG_STRING,
    CFG_KEYBIND
} ConfigType;

typedef union ConfigValue {
    bool b; // For boolean values
    int i; // For integer values
    char s[4096]; // For string values 4096 should be adequate for all use cases
    // More can be added here in the future if needed
} ConfigValue;

typedef struct ConfigEntry {
    const char* const key;
    const ConfigType type;
    ConfigValue value; // Serves as default value unless explicitly changed by user configuration
    const char* const desc;
} ConfigEntry;

typedef struct LibreSplitConfig {
    ConfigEntry start_decorated;
    ConfigEntry start_on_top;
    ConfigEntry hide_cursor;
    ConfigEntry auto_splitter_enabled;
    ConfigEntry global_hotkeys;
    ConfigEntry theme;
    ConfigEntry theme_variant;
    ConfigEntry decimals;
    ConfigEntry save_run_history;
} LibreSplitConfig;

typedef struct KeybindConfig {
    ConfigEntry start_split;
    ConfigEntry stop_reset;
    ConfigEntry cancel;
    ConfigEntry unsplit;
    ConfigEntry skip_split;
    ConfigEntry toggle_decorations;
    ConfigEntry toggle_win_on_top;
} KeybindConfig;

typedef struct HistoryConfig {
    ConfigEntry split_file;
    ConfigEntry last_split_folder;
    ConfigEntry last_auto_splitter_folder;
    ConfigEntry auto_splitter_file;
} HistoryConfig;

typedef struct AppConfig {
    LibreSplitConfig libresplit;
    KeybindConfig keybinds;
    HistoryConfig history;
} AppConfig;

/* For each section we point at the first `ConfigEntry` member inside the
 * corresponding config struct. The ConfigEntry members are declared in
 * `definitions.c` consecutively, so we can treat them as an array and
 * iterate by index to avoid repeating every field name. */
typedef struct SectionInfo {
    const char* const name;
    void* entries; /* pointer to first ConfigEntry in the section */
    const size_t count;
    const bool in_gui;
} SectionInfo;

extern const SectionInfo sections[];
extern const size_t sections_count;
