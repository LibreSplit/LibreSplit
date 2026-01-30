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
            .desc = "Start LibreSplit with WM decorations",
        },
        .start_on_top = {
            .key = "start_on_top",
            .type = CFG_BOOL,
            .value.b = true,
            .desc = "Start LibreSplit Always on Top",
        },
        .hide_cursor = {
            .key = "hide_cursor",
            .type = CFG_BOOL,
            .value.b = false,
            .desc = "Hide cursor when it's over LibreSplit",
        },
        .auto_splitter_enabled = {
            .key = "auto_splitter_enabled",
            .type = CFG_BOOL,
            .value.b = true,
            .desc = "Enable Auto Splitter",
        },
        .global_hotkeys = {
            .key = "global_hotkeys",
            .type = CFG_BOOL,
            .value.b = false,
            .desc = "Enable Global Hotkeys",
        },
        .theme = {
            .key = "theme",
            .type = CFG_STRING,
            .value.s = "standard",
            .desc = "Theme name",
        },
        .theme_variant = {
            .key = "theme_variant",
            .type = CFG_STRING,
            .value.s = "",
            .desc = "Theme Variant (if any)",
        },
        .decimals = {
            .key = "timer_decimals",
            .type = CFG_INT,
            .value.i = 2,
            .desc = "Decimals shown on timer",
        },
        .save_run_history = {
            .key = "save_run_history",
            .type = CFG_BOOL,
            .value.b = true,
            .desc = "Save run history JSON files",
        },
    },
    .keybinds = {
        .start_split = {
            .key = "start_split",
            .type = CFG_KEYBIND,
            .value.s = "space",
            .desc = "Start or Split",
        },
        .stop_reset = {
            .key = "stop_reset",
            .type = CFG_KEYBIND,
            .value.s = "BackSpace",
            .desc = "Stop or Reset Timer",
        },
        .cancel = {
            .key = "cancel",
            .type = CFG_KEYBIND,
            .value.s = "Delete",
            .desc = "Cancel Run",
        },
        .unsplit = {
            .key = "unsplit",
            .type = CFG_KEYBIND,
            .value.s = "Page_Up",
            .desc = "Undo Split",
        },
        .skip_split = {
            .key = "skip_split",
            .type = CFG_KEYBIND,
            .value.s = "Page_Down",
            .desc = "Skip Split",
        },
        .toggle_decorations = {
            .key = "toggle_decorations",
            .type = CFG_KEYBIND,
            .value.s = "Control_R",
            .desc = "Toggle Window Decorations",
        },
        .toggle_win_on_top = {
            .key = "toggle_win_on_top",
            .type = CFG_KEYBIND,
            .value.s = "<Control>k",
            .desc = "Toggle Always On Top",
        },
    },
    .history = {
        .split_file = {
            .key = "split_file",
            .type = CFG_STRING,
            .value.s = "",
            .desc = "Last Split File",
        },
        .last_split_folder = {
            .key = "last_split_folder",
            .type = CFG_STRING,
            .value.s = "",
            .desc = "Last Split Folder Opened",
        },
        .last_auto_splitter_folder = {
            .key = "last_auto_splitter_folder",
            .type = CFG_STRING,
            .value.s = "",
            .desc = "Last Auto Splitter Folder Opened",
        },
        .auto_splitter_file = {
            .key = "auto_splitter_file",
            .type = CFG_STRING,
            .value.s = "",
            .desc = "Last Auto Splitter File",
        },
    },
};

const SectionInfo sections[] = {
    { "libresplit", &cfg.libresplit, sizeof(cfg.libresplit) / sizeof(ConfigEntry), true },
    { "keybinds", &cfg.keybinds, sizeof(cfg.keybinds) / sizeof(ConfigEntry), true },
    { "history", &cfg.history, sizeof(cfg.history) / sizeof(ConfigEntry), false },
};

const size_t sections_count = sizeof(sections) / sizeof(sections[0]);
