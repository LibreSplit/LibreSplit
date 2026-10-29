#pragma once

#include <gtk/gtk.h>

typedef struct LSIcon {
    GdkDisplay* display;
    GCancellable* cancellable;
    GPtrArray* providers;
} LSIcon;

void add_class(GtkWidget* widget, const char* class);
void remove_class(GtkWidget* widget, const char* class);
char* icon_uri(const char* path, const char* source);
void append_quoted_uri(GString* str, const char* value);
LSIcon* ls_icon_new(GdkDisplay* display, const char* base_css);
void ls_icon_add(LSIcon* group, const char* path, const char* source, const char* selector);
void ls_icon_free(LSIcon* group);