#pragma once

#include <gtk/gtk.h>

void add_class(GtkWidget* widget, const char* class);

void remove_class(GtkWidget* widget, const char* class);

char* icon_uri(const char* path, const char* source);

void append_quoted_uri(GString* str, const char* value);