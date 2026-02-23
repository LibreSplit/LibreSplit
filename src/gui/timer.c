#include "timer.h"
#include "game.h"
#include "src/gui/component/components.h"
#include "src/lasr/utils.h"
#include "src/timer.h"

/**
 * Stops the timer if it's running, otherwise resets it. If the timer is reset, the current run will be saved to history if enabled.
 *
 * @param win The LibreSplit window
 */
void timer_stop_and_reset(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running) {
        ls_timer_stop(win->timer);
    }

    if (ls_timer_reset(win->timer)) {
        ls_app_window_clear_game(win);
        ls_app_window_show_game(win);
        save_game(win->game);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->stop_reset) {
            component->ops->stop_reset(component, win->timer);
        }
    }
}

/**
 * Starts the timer, if it's not already running. If the timer is already running, it does a split.
 *
 * @param win The LibreSplit window
 */
void timer_start_split(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (!win->timer->started) { // To start again a reset needs to happen
        if (ls_timer_start(win->timer)) {
            save_game(win->game);
        }
    } else {
        ls_timer_split(win->timer);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->start_split) {
            component->ops->start_split(component, win->timer);
        }
    }
}

/**
 * Starts the timer, if it's not already running. If the timer is already running, it does nothing.
 *
 * @param win The LibreSplit window
 */
void timer_start(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running)
        return; // Timer is already running, do nothing

    if (ls_timer_start(win->timer)) {
        save_game(win->game);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->start_split) {
            component->ops->start_split(component, win->timer);
        }
    }
}

/**
 * Stops the timer if it's running, otherwise resets it. If the timer is reset, the current run will be saved to history if enabled.
 *
 * @param win The LibreSplit window
 */
void timer_stop_or_reset(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running) {
        ls_timer_stop(win->timer);
    } else {
        // Restart LASR on reset
        restart_auto_splitter();

        if (ls_timer_reset(win->timer)) {
            ls_app_window_clear_game(win);
            ls_app_window_show_game(win);
            save_game(win->game);
        }
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->stop_reset) {
            component->ops->stop_reset(component, win->timer);
        }
    }
}

/**
 * @brief Cancels the current run, resetting the timer and game state and saving the cancelled run to history if enabled.
 *
 * @param win The LibreSplit window
 */
void timer_cancel_run(LSAppWindow* win)
{

    if (!win->timer)
        return;

    if (ls_timer_cancel(win->timer)) {
        ls_app_window_clear_game(win);
        ls_app_window_show_game(win);
        save_game(win->game);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->cancel_run) {
            component->ops->cancel_run(component, win->timer);
        }
    }
}

/**
 * Skips a timer split, filling the skipped split with 0's
 *
 * @param win The LibreSplit window
 */
void timer_skip(LSAppWindow* win)
{
    if (!win->timer)
        return;

    ls_timer_skip(win->timer);
    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->skip) {
            component->ops->skip(component, win->timer);
        }
    }
}

/**
 * Unsplits the last made split. If the timer is not running or if there are no splits to unsplit, it does nothing.
 *
 * @param win The LibreSplit window
 */
void timer_unsplit(LSAppWindow* win)
{
    if (!win->timer)
        return;

    ls_timer_unsplit(win->timer);

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->unsplit) {
            component->ops->unsplit(component, win->timer);
        }
    }
}

/**
 * Makes a timer split. If the timer is not running, it does nothing.
 *
 * @param win The LibreSplit window
 */
void timer_split(LSAppWindow* win)
{
    if (!win->timer)
        return;

    ls_timer_split(win->timer);

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->start_split) {
            component->ops->start_split(component, win->timer);
        }
    }
}

/**
 * Pauses the timer into a paused/loading state
 *
 * @param win The LibreSplit window
 */
void timer_pause(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running) {
        ls_timer_pause(win->timer);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->pause) {
            component->ops->pause(component, win->timer);
        }
    }
}

/**
 * Resumes the timer from a paused/loading state
 *
 * @param win The LibreSplit window
 */
void timer_unpause(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running) {
        ls_timer_unpause(win->timer);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->unpause) {
            component->ops->unpause(component, win->timer);
        }
    }
}

/**
 * Stops the timer from ticking
 *
 * @param win TheLibreSplit window
 */
void timer_stop(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (win->timer->running) {
        ls_timer_stop(win->timer);
    }

    for (GList* l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->stop_reset) {
            component->ops->stop_reset(component, win->timer);
        }
    }
}
