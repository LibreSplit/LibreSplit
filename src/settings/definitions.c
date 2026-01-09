#include "definitions.h"

/**
 * Main configuration structure with default values.
 * Default values are stored here and overridden when loading
 * JSON values information is also here
 */
AppConfig cfg = {
    .libresplit = {
        .start_decorated = {
            .key = "start_decorated",
            .type = CFG_BOOL,
            .value.b = false,
        },
        .start_on_top = {
            .key = "start_on_top",
            .type = CFG_BOOL,
            .value.b = true,
        },
        .hide_cursor = {
            .key = "hide_cursor",
            .type = CFG_BOOL,
            .value.b = false,
        },
        .auto_splitter_enabled = {
            .key = "auto_splitter_enabled",
            .type = CFG_BOOL,
            .value.b = true,
        },
        .global_hotkeys = {
            .key = "global_hotkeys",
            .type = CFG_BOOL,
            .value.b = false,
        },
        .theme = {
            .key = "theme",
            .type = CFG_STRING,
            .value.s = "standard",
        },
        .theme_variant = {
            .key = "theme_variant",
            .type = CFG_STRING,
            .value.s = "",
        },
    },
    .keybinds = {
        .start_split = {
            .key = "start_split",
            .type = CFG_STRING,
            .value.s = "space",
        },
        .stop_reset = {
            .key = "stop_reset",
            .type = CFG_STRING,
            .value.s = "BackSpace",
        },
        .cancel = {
            .key = "cancel",
            .type = CFG_STRING,
            .value.s = "Delete",
        },
        .unsplit = {
            .key = "unsplit",
            .type = CFG_STRING,
            .value.s = "Page_Up",
        },
        .skip_split = {
            .key = "skip_split",
            .type = CFG_STRING,
            .value.s = "Page_Down",
        },
        .toggle_decorations = {
            .key = "toggle_decorations",
            .type = CFG_STRING,
            .value.s = "Control_R",
        },
        .toggle_win_on_top = {
            .key = "toggle_win_on_top",
            .type = CFG_STRING,
            .value.s = "<Control>k",
        },
    },
    .history = {
        .split_file = {
            .key = "split_file",
            .type = CFG_STRING,
            .value.s = "",
        },
        .last_split_folder = {
            .key = "last_split_folder",
            .type = CFG_STRING,
            .value.s = "",
        },
        .last_auto_splitter_folder = {
            .key = "last_auto_splitter_folder",
            .type = CFG_STRING,
            .value.s = "",
        },
        .auto_splitter_file = {
            .key = "auto_splitter_file",
            .type = CFG_STRING,
            .value.s = "",
        },
    },
};

SectionInfo sections[] = {
    { "libresplit", &cfg.libresplit, sizeof(cfg.libresplit) / sizeof(ConfigEntry) },
    { "keybinds", &cfg.keybinds, sizeof(cfg.keybinds) / sizeof(ConfigEntry) },
    { "history", &cfg.history, sizeof(cfg.history) / sizeof(ConfigEntry) },
};

const size_t sections_count = sizeof(sections) / sizeof(sections[0]);
