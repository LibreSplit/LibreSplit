#pragma once

#include "src/settings/definitions.h"
#include <gtk/gtk.h>

extern AppConfig cfg;

/**
 * @brief A GUI connector between a widget and a ConfigEntry
 */
typedef struct LSGuiSetting {
    GtkWidget* widget; /*!< The widget used to change the ConfigEntry */
    GtkEntryBuffer* entry_buffer; /*!< The Entry Buffer used to hold string data for certain ConfigEntry */
    ConfigEntry* settings_entry; /*!< The ConfigEntry that is changed by the widget. */
} LSGuiSetting;

void show_settings_dialog(GSimpleAction* action, GVariant* parameter, gpointer app);
