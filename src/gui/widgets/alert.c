#include "alert.h"

/**
 * @brief Presents an error alert to the user
 *
 * @param parent The parent window for this alert
 * @param title The alert title that appears on the titlebar
 * @param message The message header
 * @param detail Optional message details
 */
void ls_alert_error(GtkWindow* parent, const char* title, const char* message, const char* detail)
{
    const LSDialogIcon icon = {
        .source = "dialog-error",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_alert(parent, title, message, detail, &icon);
}

/**
 * @brief Presents a warning alert to the user
 *
 * @param parent The parent window for this alert
 * @param title The alert title that appears on the titlebar
 * @param message The message header
 * @param detail Optional message details
 */
void ls_alert_warning(GtkWindow* parent, const char* title, const char* message, const char* detail)
{
    const LSDialogIcon icon = {
        .source = "dialog-warning",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_alert(parent, title, message, detail, &icon);
}

/**
 * @brief Presents an informational alert to the user
 *
 * @param parent The parent window for this alert
 * @param title The alert title that appears on the titlebar
 * @param message The message header
 * @param detail Optional message details
 */
void ls_alert_info(GtkWindow* parent, const char* title, const char* message, const char* detail)
{
    const LSDialogIcon icon = {
        .source = "dialog-information",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_alert(parent, title, message, detail, &icon);
}

/**
 * @brief Create an alert-style window with any or no icon
 *
 * @param parent The parent window for this alert
 * @param title The alert title that appears on the titlebar
 * @param message The message header
 * @param detail Optional message details
 * @param icon Optional icon to display on the alert
 */
void ls_alert(GtkWindow* parent, const char* title, const char* message, const char* detail, LSDialogIcon* icon)
{
    const LSDialogOption options[] = {
        {
            .label = "_OK",
            .callback = NULL,
            .is_cancel = FALSE,
            .is_default = TRUE,
        }
    };

    ls_dialog_open(parent, title, message, detail, options, icon, G_N_ELEMENTS(options), NULL, NULL);
}