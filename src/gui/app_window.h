#pragma once

#include "src/gui/welcome_box.h"
#include "src/keybinds/keybinds.h"
#include "src/opts.h"
#include "src/timer.h"
#include <gtk/gtk.h>

typedef struct _LSAppWindowClass {
    GtkApplicationWindowClass parent_class;
} LSAppWindowClass;

typedef struct LSApp {
    GtkApplication parent;
} LSApp;

typedef struct _LSAppClass {
    GtkApplicationClass parent_class;
} LSAppClass;

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
    GList* components;
    GtkWidget* footer;
    GtkCssProvider* style; // Current style provider, there can be only one
    LSKeybinds keybinds; /*!< The keybinds related to this application window */
    LSOpts opts; /*!< The window options */
} LSAppWindow;
