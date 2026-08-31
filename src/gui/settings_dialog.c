#include "settings_dialog.h"
#include "app_window.h"
#include "src/logging.h"
#include "src/gui/widgets/alert.h"
#include "src/settings/definitions.h"
#include "src/settings/settings.h"

#include <gdk/gdk.h>
#include <glib-object.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <stdio.h>

static LSGuiSetting* gui_settings = NULL;

static GtkWidget* settings_window_singleton = NULL;

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

static void set_widget_defaults(GtkWidget* obj)
{
    gtk_widget_set_margin_top(obj, 8);
    gtk_widget_set_margin_bottom(obj, 8);
    gtk_widget_set_margin_start(obj, 8);
    gtk_widget_set_margin_end(obj, 8);
    gtk_widget_set_vexpand(obj, TRUE);
    gtk_widget_set_hexpand(obj, TRUE);
}

static void build_settings_dialog(GtkApplication* app, gpointer data)
{
    LOG_INFO("Creating the settings dialog...");
    // Show already open window if another one is called.
    if (settings_window_singleton) {
        gtk_window_present(GTK_WINDOW(settings_window_singleton));
        return;
    }

    int settings_number = enumerate_settings(cfg);
    gui_settings = malloc(settings_number * sizeof(LSGuiSetting));
    if (gui_settings == NULL) {
        LOG_WARN("Cannot allocate memory for the settings GUI.");
        return;
    }

    GtkWindow* parent = gtk_application_get_active_window(app);
    GtkWidget* window = gtk_dialog_new_with_buttons("LibreSplit Settings", parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL, NULL);
    gtk_window_set_application(GTK_WINDOW(window), app);
    settings_window_singleton = window;
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(window, "destroy", G_CALLBACK(on_settings_window_destroy), NULL);
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    set_widget_defaults(main_box);
    GtkWidget* tabs = gtk_notebook_new();
    set_widget_defaults(tabs);
    int settings_idx = 0;
    for (size_t s = 0; s < sections_count; ++s) {
        SectionInfo section_info = sections[s];
        if (!section_info.in_gui) {
            continue;
        }
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        set_widget_defaults(box);
        GtkWidget* title = gtk_label_new(section_info.name);
        for (size_t i = 0; i < section_info.count; ++i) {
            ConfigEntry entry = ((ConfigEntry*)section_info.entries)[i];
            gui_settings[settings_idx].settings_entry = &((ConfigEntry*)section_info.entries)[i];
            switch (entry.type) {
                case CFG_STRING:
                    {
                        GtkWidget* lbl_str = gtk_label_new(entry.desc);
                        gtk_box_append(GTK_BOX(box), lbl_str);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(entry.value.s, sizeof(entry.value.s));
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(gui_settings[settings_idx].widget), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
                        g_signal_connect(gui_settings[settings_idx].widget, "icon-press", G_CALLBACK(on_entry_clear_press), NULL);
                        gtk_box_append(GTK_BOX(box), gui_settings[settings_idx].widget);
                        break;
                    }
                case CFG_KEYBIND:
                    {
                        GtkWidget* lbl_kb = gtk_label_new(entry.desc);
                        gtk_box_append(GTK_BOX(box), lbl_kb);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(entry.value.s, sizeof(entry.value.s));
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(gui_settings[settings_idx].widget), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
                        GtkEventController* key_controller = gtk_event_controller_key_new();
                        gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
                        g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press), NULL);
                        gtk_widget_add_controller(gui_settings[settings_idx].widget, key_controller);
                        g_signal_connect(gui_settings[settings_idx].widget, "icon-press", G_CALLBACK(on_entry_clear_press), NULL);
                        gtk_box_append(GTK_BOX(box), gui_settings[settings_idx].widget);
                        break;
                    }
                case CFG_BOOL:
                    {
                        gui_settings[settings_idx].widget = gtk_check_button_new_with_label(entry.desc);
                        gtk_check_button_set_active(GTK_CHECK_BUTTON(gui_settings[settings_idx].widget), entry.value.b);
                        gtk_box_append(GTK_BOX(box), gui_settings[settings_idx].widget);
                        break;
                    }
                case CFG_INT:
                    {
                        GtkWidget* lbl_int = gtk_label_new(entry.desc);
                        gtk_box_append(GTK_BOX(box), lbl_int);
                        char setting_as_str[64];
                        sprintf(setting_as_str, "%d", entry.value.i);

                        gui_settings[settings_idx].entry_buffer = gtk_entry_buffer_new(setting_as_str, sizeof(setting_as_str));
                        gui_settings[settings_idx].widget = gtk_entry_new_with_buffer(gui_settings[settings_idx].entry_buffer);
                        gtk_box_append(GTK_BOX(box), gui_settings[settings_idx].widget);
                        break;
                    }
            }
            settings_idx++;
        }
        gtk_notebook_append_page(GTK_NOTEBOOK(tabs), box, title);
    }
    gtk_box_append(GTK_BOX(main_box), tabs);
    GtkWidget* save_btn = gtk_button_new_with_label("Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(save_gui_settings), parent);
    gtk_box_append(GTK_BOX(main_box), save_btn);
    gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(window))), main_box);
    gtk_window_present(GTK_WINDOW(window));
    ls_alert_open(GTK_WINDOW(window), "Settings Alert", "Settings", "Welcome to our nifty settings thing!\n" "This alert is totally not annoying and completely unnecessary at all! c:");
}

void show_settings_dialog(GSimpleAction* action, GVariant* parameter, gpointer app)
{
    if (parameter != NULL) {
        app = parameter;
    }
    build_settings_dialog(app, NULL);
}
