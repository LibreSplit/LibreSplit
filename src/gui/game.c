#include "game.h"
#include "src/gui/component/components.h"
#include "src/gui/theming.h"
#include "src/lasr/auto-splitter.h"
#include "src/settings/definitions.h"

extern AppConfig cfg;

/**
 * Clears the current game and reset all the components.
 *
 * @param win The LibreSplit app window
 */
void ls_app_window_clear_game(LSAppWindow* win)
{
    GList* l;

    atomic_store(&run_finished, false);

    gtk_widget_hide(win->box);
    gtk_widget_show_all(win->welcome_box->box);

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->clear_game) {
            component->ops->clear_game(component);
        }
    }

    ls_app_load_theme_with_fallback(win, cfg.libresplit.theme.value.s, cfg.libresplit.theme_variant.value.s);
}

/**
 * Prepares the LibreSplit window to be shown, using the data
 * from the loaded split file.
 *
 * @param win The LibreSplit window.
 */
void ls_app_window_show_game(LSAppWindow* win)
{
    GList* l;

    // set dimensions
    if (win->game->width > 0 && win->game->height > 0) {
        gtk_widget_set_size_request(GTK_WIDGET(win),
            win->game->width,
            win->game->height);
    }

    // set game theme (if it is set)
    if (win->game->theme) {
        ls_app_load_theme_with_fallback(win, win->game->theme, win->game->theme_variant);
    }

    for (l = win->components; l != NULL; l = l->next) {
        LSComponent* component = l->data;
        if (component->ops->show_game) {
            component->ops->show_game(component, win->game, win->timer);
        }
    }

    gtk_widget_show(win->box);
    gtk_widget_hide(win->welcome_box->box);
}

gpointer save_game_thread(gpointer data)
{
    ls_game* game = data;
    ls_game_save(game);
    return NULL;
}

void save_game(ls_game* game)
{
    GThread* thread = g_thread_new("save_game", save_game_thread, game);
    g_thread_unref(thread);
}
