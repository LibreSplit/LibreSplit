#pragma once

#include <gtk/gtk.h>
#include <stdbool.h>

#define LIBRESPLIT_RESOURCES_PREFIX "/org/libresplit/LibreSplit/"

void get_libresplit_data_folder_path(char* out_path);
void get_libresplit_folder_path(char* out_path);
void check_directories(void);
bool create_default_directory(const char* name, const char* path, mode_t permissions, GtkWindow* parent);
