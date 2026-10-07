#pragma once

#include <gdk/gdk.h>

/**
 * @brief Keybind A GTK Key bind
 */
typedef struct
{
    guint key; /*!< The key value */
    GdkModifierType mods; /*!< The modifiers used (shift, ctrl, ...) */
} Keybind;

/**
 * @brief The keybinds associated with LibreSplit's window
 */
typedef struct {
    Keybind start_split; /*!< The "start or split" global keybind */
    Keybind stop_reset; /*!< The "stop or reset timer" global keybind */
    Keybind cancel; /*!< The "cancel" global keybind */
    Keybind unsplit; /*!< The "undo split" global keybind */
    Keybind skip_split; /*!< The "skip split" global keybind */
    Keybind toggle_decorations; /*!< The "toggle decorations" global keybind */
    Keybind toggle_win_on_top; /*!< The "always-on-top" global keybind */
} LSKeybinds;

Keybind parse_keybind(const gchar* accelerator);
