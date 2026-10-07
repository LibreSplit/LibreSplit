#pragma once

#include "dialog.h"

void ls_alert_error(GtkWindow* parent, const char* title, const char* message, const char* detail);
void ls_alert_warning(GtkWindow* parent, const char* title, const char* message, const char* detail);
void ls_alert_info(GtkWindow* parent, const char* title, const char* message, const char* detail);
void ls_alert(GtkWindow* parent, const char* title, const char* message, const char* detail, const LSDialogIcon* icon);
