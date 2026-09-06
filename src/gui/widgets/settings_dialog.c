#include "settings_dialog.h"
#include "alert.h"
#include "src/gui/app_window.h"
#include "src/logging.h"
#include "src/settings/definitions.h"
#include "src/settings/settings.h"

#include <gdk/gdk.h>
#include <glib-object.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <stdio.h>

static LSGuiSetting* gui_settings = NULL;

static GtkWindow* settings_window_singleton = NULL;

/**
 * Takes the application config and counts how many settings are available.
 *
 * This is used to create space for the dynamic settings window.
 *
 * @param cfg The LibreSplit AppConfig instance.
 *
 * @return The number of settings available.
 */
static size_t enumerate_settings(AppConfig cfg)
{
    LOG_DEBUG("Enumerating settings to add to the GUI...");
    int settings_number = 0;
    for (size_t s = 0; s < sections_count; ++s) {
        SectionInfo section_info = sections[s];
        if (!section_info.in_gui) {
            continue;
        }
        settings_number += section_info.count;
    }

    return settings_number;
}

/**
 * Frees memory when the settings dialog is destroyed.
 *
 * @param widget The Window itself
 * @param user_data unused
 */
static void on_settings_window_destroy(GtkWidget* widget, gpointer user_data)
{
    LOG_DEBUG("Destroying the settings window...");
    settings_window_singleton = NULL;
    free(gui_settings);
    gui_settings = NULL;
    dialog_count_dec();
}

/**
 * Closes the settings window when Escape is pressed.
 *
 * @param controller The key controller attached to the settings dialog.
 * @param keyval The pressed key value.
 * @param keycode The pressed hardware keycode.
 * @param state The active keyboard modifiers.
 * @param data Unused.
 *
 * @return Whether the key event was handled.
 */
static gboolean close_settings_window_on_escape(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    if (keyval != GDK_KEY_Escape) {
        return FALSE;
    }

    gtk_window_close(GTK_WINDOW(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller))));
    return TRUE;
}

/**
 * Converts a combination of Modifiers and a keyval into a gsettings string for key binds.
 *
 * @param keyval The value of the Key pressed.
 * @param modifiers The modifiers that are pressed.
 * @param buffer The buffer to write the final string into.
 * @param buffer_size The destination buffer size
 */
static void get_key_string(gint keyval, GdkModifierType modifiers, char* buffer, size_t buffer_size)
{
    const char* key_name = gdk_keyval_name(gdk_keyval_to_lower(keyval));
    char str_modifiers[64];

    str_modifiers[0] = '\0';

    // Process modifiers
    if (modifiers & GDK_CONTROL_MASK) {
        strcat(str_modifiers, "<Control>");
    }
    if (modifiers & GDK_SHIFT_MASK) {
        strcat(str_modifiers, "<Shift>");
    }
    if (modifiers & GDK_ALT_MASK) {
        strcat(str_modifiers, "<Alt>");
    }
    if (modifiers & GDK_SUPER_MASK) {
        strcat(str_modifiers, "<Super>");
    }
    if (modifiers & GDK_HYPER_MASK) {
        strcat(str_modifiers, "<Hyper>");
    }
    snprintf(buffer, buffer_size, "%s%s", str_modifiers, key_name);
}

/**
 * Handler for key press events on "Key Grabber" entries.
 *
 * @param controller The key controller attached to the entry widget
 * @param keyval The pressed key value
 * @param keycode The pressed hardware keycode
 * @param state The active keyboard modifiers
 * @param data unused
 *
 * @return True if the handler terminated correctly.
 */
gboolean on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    char key_buffer[128];
    get_key_string(keyval, state, key_buffer, sizeof(key_buffer));
    gtk_editable_set_text(
        GTK_EDITABLE(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller))),
        key_buffer);
    return TRUE;
}

/**
 * Handler for clicking on the "clear icon" on Entry fields.
 *
 * @param widget The entry widget
 * @param icon_pos The position of the icon clicked (unused).
 * @param data unused
 */
void on_entry_clear_press(GtkEntry* widget, GtkEntryIconPosition icon_pos, gpointer data)
{
    gtk_editable_set_text(GTK_EDITABLE(widget), "");
}

/**
 * Takes the values from the GUI and saves them back into the program settings.
 * @param action The action performed (unused).
 * @param parameter Parameters to the action (unused).
 * @param app The LibreSplit Application pointer (unused).
 */
static void save_gui_settings(GtkButton* button, gpointer app)
{
    LOG_INFO("Saving settings from the GUI...");
    size_t settings_number = enumerate_settings(cfg);
    // Parse all values in gui_settings, assign them to the respective cfg settings
    for (size_t i = 0; i < settings_number; i++) {
        LSGuiSetting setting_to_save = gui_settings[i];
        switch (setting_to_save.settings_entry->type) {
            case CFG_STRING:
            case CFG_KEYBIND:
                {
                    const char* str_value = gtk_entry_buffer_get_text(setting_to_save.entry_buffer);
                    if (strlen(str_value) >= sizeof(setting_to_save.settings_entry->value.s)) {
                        LOG_WARNF("%s was longer than the max size %zu", str_value, sizeof(setting_to_save.settings_entry->value.s));
                        break;
                    }

                    strcpy(setting_to_save.settings_entry->value.s, str_value);
                    break;
                }
            case CFG_BOOL:
                {
                    bool bool_value = gtk_check_button_get_active(GTK_CHECK_BUTTON(setting_to_save.widget));
                    setting_to_save.settings_entry->value.b = bool_value;
                    break;
                }
            case CFG_INT:
                {
                    const char* int_str_value = gtk_entry_buffer_get_text(setting_to_save.entry_buffer);
                    int int_value = atoi(int_str_value);
                    setting_to_save.settings_entry->value.i = int_value;
                    break;
                }
        }
    }

    // Call the normal save_settings thing
    if (config_save()) {
        // on success, set decorations in case the setting changed.
        LSAppWindow* win = LS_APP_WINDOW(app);
        win->opts.decorated = cfg.libresplit.start_decorated.value.b;
        set_window_decorations(win);
    }
}

/**
 * Creates the label for a setting.
 *
 * @param text The setting description.
 * @return The label.
 */
static GtkWidget* new_setting_label(const char* text)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    return label;
}

/**
 * Builds the settings dialog.
 *
 * @param data The LibreSplit GTK Application
 * @return Whether or not to remove this function from the queue
 */
static gboolean build_settings_dialog(gpointer data)
{
    if (settings_window_singleton) {
        gtk_window_present(settings_window_singleton);
        return G_SOURCE_REMOVE;
    }

    LOG_INFO("Creating the settings dialog...");
    GtkApplication* app = GTK_APPLICATION(data);
    LSAppWindow* win = ls_get_main_app_window(app);
    if (win == NULL) {
        LOG_ERR("Main application window was not found");
        return G_SOURCE_REMOVE;
    }

    GtkWindow* parent = GTK_WINDOW(win);
    int settings_number = enumerate_settings(cfg);
    gui_settings = malloc(settings_number * sizeof(LSGuiSetting));
    if (gui_settings == NULL) {
        LOG_WARN("Cannot allocate memory for the settings GUI.");
        return G_SOURCE_REMOVE;
    }

    dialog_count_inc();
    set_main_window_keep_above(FALSE);

    GtkWindow* window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, "LibreSplit Settings");
    gtk_window_set_application(window, app);
    gtk_window_set_modal(window, TRUE);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_default_size(window, 500, 500);
    gtk_window_set_transient_for(window, parent);
    gtk_window_set_destroy_with_parent(window, TRUE);

    settings_window_singleton = window;
    g_signal_connect(window, "destroy", G_CALLBACK(on_settings_window_destroy), NULL);

    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(close_settings_window_on_escape), NULL);
    gtk_widget_add_controller(GTK_WIDGET(window), key_controller);

    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, DIALOG_CONTENT_SPACING);
    gtk_widget_set_margin_top(main_box, DIALOG_MARGIN);
    gtk_widget_set_margin_bottom(main_box, DIALOG_MARGIN);
    gtk_widget_set_margin_start(main_box, DIALOG_MARGIN);
    gtk_widget_set_margin_end(main_box, DIALOG_MARGIN);
    gtk_window_set_child(window, main_box);

    GtkWidget* tabs = gtk_notebook_new();
    gtk_widget_set_hexpand(tabs, TRUE);
    gtk_widget_set_vexpand(tabs, TRUE);
    int settings_idx = 0;
    for (size_t s = 0; s < sections_count; ++s) {
        SectionInfo section_info = sections[s];
        if (!section_info.in_gui) {
            continue;
        }

        GtkWidget* page = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(page), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_hexpand(page, TRUE);
        gtk_widget_set_vexpand(page, TRUE);

        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), DIALOG_ROW_SPACING);
        gtk_grid_set_column_spacing(GTK_GRID(grid), DIALOG_COLUMN_SPACING);
        gtk_widget_set_margin_top(grid, DIALOG_INNER_MARGIN);
        gtk_widget_set_margin_bottom(grid, DIALOG_INNER_MARGIN);
        gtk_widget_set_margin_start(grid, DIALOG_INNER_MARGIN);
        gtk_widget_set_margin_end(grid, DIALOG_INNER_MARGIN);
        gtk_widget_set_hexpand(grid, TRUE);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(page), grid);

        GtkWidget* title = gtk_label_new(section_info.name);
        int row = 0;
        for (size_t i = 0; i < section_info.count; ++i) {
            ConfigEntry entry = ((ConfigEntry*)section_info.entries)[i];
            gui_settings[settings_idx].settings_entry = &((ConfigEntry*)section_info.entries)[i];
            switch (entry.type) {
                case CFG_STRING:
                    {
                        GtkWidget* lbl_str = new_setting_label(entry.desc);
                        gtk_grid_attach(GTK_GRID(grid), lbl_str, 0, row, 1, 1);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(entry.value.s, -1);
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        g_object_unref(gui_settings[settings_idx].entry_buffer);
                        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(gui_settings[settings_idx].widget), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
                        g_signal_connect(gui_settings[settings_idx].widget, "icon-press", G_CALLBACK(on_entry_clear_press), NULL);
                        gtk_widget_set_hexpand(gui_settings[settings_idx].widget, TRUE);
                        gtk_grid_attach(GTK_GRID(grid), gui_settings[settings_idx].widget, 1, row, 1, 1);
                        break;
                    }
                case CFG_KEYBIND:
                    {
                        /*! TODO: Unbind logic and buttons */
                        GtkWidget* lbl_kb = new_setting_label(entry.desc);
                        gtk_grid_attach(GTK_GRID(grid), lbl_kb, 0, row, 1, 1);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(entry.value.s, -1);
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        g_object_unref(gui_settings[settings_idx].entry_buffer);
                        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(gui_settings[settings_idx].widget), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
                        GtkEventController* key_controller = gtk_event_controller_key_new();
                        gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
                        g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
                        gtk_widget_add_controller(gui_settings[settings_idx].widget, key_controller);
                        g_signal_connect(gui_settings[settings_idx].widget, "icon-press", G_CALLBACK(on_entry_clear_press), NULL);
                        gtk_widget_set_hexpand(gui_settings[settings_idx].widget, TRUE);
                        gtk_grid_attach(GTK_GRID(grid), gui_settings[settings_idx].widget, 1, row, 1, 1);
                        break;
                    }
                case CFG_BOOL:
                    {
                        gui_settings[settings_idx].widget = gtk_check_button_new_with_label(entry.desc);
                        gtk_check_button_set_active(GTK_CHECK_BUTTON(gui_settings[settings_idx].widget), entry.value.b);
                        gtk_widget_set_halign(gui_settings[settings_idx].widget, GTK_ALIGN_START);
                        gtk_grid_attach(GTK_GRID(grid), gui_settings[settings_idx].widget, 0, row, 2, 1);
                        break;
                    }
                case CFG_INT:
                    {
                        GtkWidget* lbl_int = new_setting_label(entry.desc);
                        gtk_grid_attach(GTK_GRID(grid), lbl_int, 0, row, 1, 1);
                        char setting_as_str[64];
                        sprintf(setting_as_str, "%d", entry.value.i);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(setting_as_str, -1);
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        g_object_unref(gui_settings[settings_idx].entry_buffer);
                        gtk_widget_set_hexpand(gui_settings[settings_idx].widget, TRUE);
                        gtk_grid_attach(GTK_GRID(grid), gui_settings[settings_idx].widget, 1, row, 1, 1);
                        break;
                    }
            }
            settings_idx++;
            row++;
        }
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), page, title);
    }
    gtk_box_append(GTK_BOX(main_box), tabs);

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(main_box), actions);

    GtkWidget* save_btn = gtk_button_new_with_label("Save");
    gtk_widget_set_size_request(save_btn, DIALOG_ACTION_WIDTH, -1);
    gtk_widget_add_css_class(save_btn, "suggested-action");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(save_gui_settings), parent);
    gtk_box_append(GTK_BOX(actions), save_btn);

    gtk_window_present(window);
    return G_SOURCE_REMOVE;
}

/**
 * Shows the settings dialog when the ContextMenu option is clicked.
 *
 * @param action The action performed.
 * @param parameter Unused
 * @param app The LibreSplit Application pointer.
 */
void show_settings_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    g_idle_add_full(G_PRIORITY_DEFAULT, build_settings_dialog, g_object_ref(app), g_object_unref);
}
