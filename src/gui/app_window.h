#pragma once

#include "src/gui/welcome_box.h"
#include "src/keybinds/delayed_handlers.h"
#include "src/keybinds/keybinds.h"
#include "src/opts.h"
#include "src/timer.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#define WINDOW_PAD (8)

G_DECLARE_FINAL_TYPE(LSApp, ls_app, LS, APP, GtkApplication)
#define LS_APP_TYPE (ls_app_get_type())
#define LS_APP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_TYPE, LSApp))

G_DECLARE_FINAL_TYPE(LSAppWindow, ls_app_window, LS, APP_WINDOW, GtkApplicationWindow)

#define LS_APP_WINDOW_TYPE (ls_app_window_get_type())
#define LS_APP_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), LS_APP_WINDOW_TYPE, LSAppWindow))

struct _LSAppWindowClass {
    GtkApplicationWindowClass parent_class;
};

struct _LSApp {
    GtkApplication parent;
};

struct _LSAppClass {
    GtkApplicationClass parent_class;
};

/**
 * @brief The main LibreSplit application window
 */
typedef struct _LSAppWindow {
    GtkApplicationWindow parent; /*!< The proper GTK base application*/
    char data_path[PATH_MAX]; /*!< The path to the libresplit user config directory */
    ls_game* game;
    ls_timer* timer;
    GdkDisplay* display;
    GtkWidget* container;
    LSWelcomeBox* welcome_box;
    GtkWidget* box;
    GtkWidget* context_menu; /*!< The context menu */
    GList* components;
    GtkWidget* footer;
    GtkCssProvider* reset_style; /*!< The "reset rules" provider, will remove desktop theme rules */
    GtkCssProvider* style; /*!< Current style provider, there can be only one */
    LSKeybinds keybinds; /*!< The keybinds related to this application window */
    DelayedHandlers delayed_handlers; /*!< Handlers due for the next window step */
    LSOpts opts; /*!< The window options */
} LSAppWindow;

void toggle_decorations(LSAppWindow* win);
void toggle_win_on_top(LSAppWindow* win);

gboolean ls_app_window_resize(GtkWidget* widget, GdkEvent* event, gpointer data);

LSAppWindow* ls_app_window_new(LSApp* app);
void ls_app_activate(GApplication* app);
void ls_app_open(GApplication* app, GFile** files, gint n_files, const gchar* hint);
LSApp* ls_app_new(void);

void ls_app_window_open(LSAppWindow* win, const char* file);

gboolean ls_app_window_step(gpointer data);
void ls_app_window_destroy(GtkWidget* widget, gpointer data);
gboolean ls_app_window_draw(gpointer data);
