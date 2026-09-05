#include "keybinds/keybinds.h"
#include <gtk/gtk.h>

/**
 * Parses a string representing a Keybind definition
 * into a Keybind structure.
 *
 * @param accelerator The string representation of the keybind.
 *
 * @return A Keybind struct corresponding to the requested keybind.
 */
Keybind parse_keybind(const gchar* accelerator)
{
    Keybind kb;
    gtk_accelerator_parse(accelerator, &kb.key, &kb.mods);
    return kb;
}
