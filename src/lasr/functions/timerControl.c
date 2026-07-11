#include "timerControl.h"

#include "../utils.h"
#include <stdatomic.h>

extern atomic_bool run_running;

/**
 * Sets run_running to false.
 *
 * @param L The lua stack
 *
 * @return Always 1.
 */
int pauseTimer()
{
    atomic_store(&run_running, false);
    return 1;
}

/**
 * Sets run_running to false.
 *
 * @param L The lua stack
 *
 * @return Always 1.
 */
int resumeTimer()
{
    atomic_store(&run_running, true);
    return 1;
}
