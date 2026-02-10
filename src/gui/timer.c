#include "timer.h"
#include "game.h"
#include "src/gui/component/components.h"
#include "src/lasr/utils.h"
#include "src/timer.h"

// Stops and then resets the timer
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

// Starts or splits the timer, whichever is appropriate
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

void timer_start(LSAppWindow* win)
{
    if (!win->timer)
        return;

    if (!win->timer->running) {
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
}

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
