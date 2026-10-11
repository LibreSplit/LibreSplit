#include "utils.h"
#include "src/logging.h"
#include <glib.h>
#include <time.h>

/**
 * @brief Sets today's date to the date buffer in YYYY-MM-DD format.
 *
 * @param date Pointer to a string of at least length 16.
 * @return bool Whether or not fetching today's date was successful.
 */
bool set_date(char* date)
{
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        LOG_WARNF("failed to set time: %s", g_strerror(errno));
        return false;
    }

    struct tm local_time;
    if (localtime_r(&now, &local_time) == NULL) {
        LOG_WARN("failed to format time in the user's locale");
        return false;
    }

    if (strftime(date, 16, "%Y-%m-%d", &local_time) == 0) {
        LOG_WARN("failed to store the formatted time in the date buffer, the result might be longer than date's size");
        return false;
    }

    return true;
}
