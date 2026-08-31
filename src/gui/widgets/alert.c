#include "alert.h"
#include "dialog.h"

// TODO: WIP Alets, but submitting this PR without finishing this in case we don't want them
void ls_alert_open(const char* title, const char* message, const char* detail)
{
    const LSDialogOption options[] = {
        {
            .label = "_OK",
            .callback = NULL,
            .is_cancel = FALSE,
            .is_default = TRUE,
        }
    };

    const LSDialogIcon icon = {
        .source = "dialog-information",
        .type = LS_DIALOG_ICON_NAME,
    };

    ls_dialog_open(title, message, detail, options, &icon, G_N_ELEMENTS(options), NULL, NULL);
}