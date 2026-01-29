#include "timer.h"
#include "game.h"
#include "src/gui/component/components.h"
#include "src/lasr/auto-splitter.h"
#include "src/timer.h"

void timer_reset(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (win->timer->running) {
            ls_timer_stop(win->timer);
            for (l = win->components; l != NULL; l = l->next) {
                LSComponent* component = l->data;
                if (component->ops->stop_reset) {
                    component->ops->stop_reset(component, win->timer);
                }
            }
        }
        if (ls_timer_reset(win->timer)) {
            ls_app_window_clear_game(win);
            ls_app_window_show_game(win);
            save_game(win->game);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}

void timer_start_split(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (!win->timer->running) {
            if (ls_timer_start(win->timer)) {
                save_game(win->game);
            }
        } else {
            timer_split(win, false);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->start_split) {
                component->ops->start_split(component, win->timer);
            }
        }
    }
}

void timer_start(LSAppWindow* win, bool updateComponents)
{
    if (win->timer) {
        GList* l;
        if (!win->timer->running) {
            if (ls_timer_start(win->timer)) {
                save_game(win->game);
            }
            if (updateComponents) {
                for (l = win->components; l != NULL; l = l->next) {
                    LSComponent* component = l->data;
                    if (component->ops->start_split) {
                        component->ops->start_split(component, win->timer);
                    }
                }
            }
        }
    }
}

void timer_stop_reset(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (is_run_started(win->timer)) {
            ls_timer_stop(win->timer);
        } else {
            const bool was_asl_enabled = atomic_load(&auto_splitter_enabled);
            atomic_store(&auto_splitter_enabled, false);
            while (atomic_load(&auto_splitter_running) && was_asl_enabled) {
                // wait, this will be very fast so its ok to just spin
            }
            if (was_asl_enabled)
                atomic_store(&auto_splitter_enabled, true);

            if (ls_timer_reset(win->timer)) {
                ls_app_window_clear_game(win);
                ls_app_window_show_game(win);
                save_game(win->game);
            }
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}

void timer_cancel_run(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (ls_timer_cancel(win->timer)) {
            ls_app_window_clear_game(win);
            ls_app_window_show_game(win);
            save_game(win->game);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->cancel_run) {
                component->ops->cancel_run(component, win->timer);
            }
        }
    }
}

void timer_skip(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        ls_timer_skip(win->timer);
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->skip) {
                component->ops->skip(component, win->timer);
            }
        }
    }
}

void timer_unsplit(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        ls_timer_unsplit(win->timer);
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->unsplit) {
                component->ops->unsplit(component, win->timer);
            }
        }
    }
}

void timer_split(LSAppWindow* win, bool updateComponents)
{
    if (win->timer) {
        GList* l;
        ls_timer_split(win->timer);
        if (updateComponents) {
            for (l = win->components; l != NULL; l = l->next) {
                LSComponent* component = l->data;
                if (component->ops->start_split) {
                    component->ops->start_split(component, win->timer);
                }
            }
        }
    }
}

void timer_stop(LSAppWindow* win)
{
    if (win->timer) {
        GList* l;
        if (win->timer->running) {
            ls_timer_stop(win->timer);
        }
        for (l = win->components; l != NULL; l = l->next) {
            LSComponent* component = l->data;
            if (component->ops->stop_reset) {
                component->ops->stop_reset(component, win->timer);
            }
        }
    }
}
