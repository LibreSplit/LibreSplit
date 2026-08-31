#include "alert.h"
#include "dialog.h"

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

    ls_dialog_open(title, message, detail, options, G_N_ELEMENTS(options), NULL, NULL);
}