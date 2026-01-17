/** \file title.c
 *
 * Implementation of the title component
 */
#include "components.h"

/**
 * @brief The component representing the title.
 *
 * Represents the title of the run, as well as the count of attempts, both finished and total.
 */
typedef struct LSTitle {
    LSComponent base; /*!< The base struct that is extended */
    GtkWidget* header; /*!< The container for the title */
    GtkWidget* title; /*!< The label containing the title itself */
    GtkWidget* attempt_count; /*!< The label containing the number of attempts. */
    GtkWidget* finished_count; /*<! The label containing the number of finished runs. */
} LSTitle;
extern LSComponentOps ls_title_operations; // defined at the end of the file

/**
 * Constructor
 */
LSComponent* ls_component_title_new()
{
    LSTitle* self;

    self = malloc(sizeof(LSTitle));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_title_operations;

    self->header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->header, "header");
    gtk_widget_show(self->header);

    self->title = gtk_label_new(NULL);
    add_class(self->title, "title");
    gtk_label_set_justify(GTK_LABEL(self->title), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(self->title), TRUE);
    gtk_widget_set_hexpand(self->title, TRUE);
    gtk_container_add(GTK_CONTAINER(self->header), self->title);

    self->attempt_count = gtk_label_new(NULL);
    add_class(self->attempt_count, "attempt-count");
    gtk_widget_set_margin_start(self->attempt_count, 8);
    gtk_widget_set_valign(self->attempt_count, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(self->header), self->attempt_count);
    gtk_widget_show(self->attempt_count);

    self->finished_count = gtk_label_new(NULL);
    add_class(self->finished_count, "finished_count");
    gtk_widget_set_margin_start(self->finished_count, 8);
    gtk_widget_set_valign(self->finished_count, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(self->header), self->finished_count);
    gtk_widget_show(self->finished_count);

    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void title_delete(LSComponent* self)
{
    free(self);
}

/**
 * Returns the Title GTK widget.
 *
 * @param self The Title component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* title_widget(LSComponent* self)
{
    return ((LSTitle*)self)->header;
}

/**
 * Function to execute when resize_window is executed (the LibreSplit window is resized).
 *
 * @param self_ The title component itself.
 * @param win_width The new window width.
 * @param win_height The new window height.
 */
static void title_resize(LSComponent* self_, int win_width, int win_height)
{
    GdkRectangle rect;
    int attempt_count_width;
    int finished_count_width;
    int title_width;
    LSTitle* self = (LSTitle*)self_;

    gtk_widget_hide(self->title);
    gtk_widget_get_allocation(self->attempt_count, &rect);
    attempt_count_width = rect.width;
    gtk_widget_get_allocation(self->finished_count, &rect);
    finished_count_width = rect.width;
    title_width = win_width - (attempt_count_width + finished_count_width);
    rect.width = title_width;
    gtk_widget_show(self->title);
    gtk_widget_set_allocation(self->title, &rect);
}

/**
 * Function to execute when ls_app_window_show_game is executed.
 *
 * @param self_ The Title component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void title_show_game(LSComponent* self_, const ls_game* game,
    const ls_timer* timer)
{
    char str[64];
    LSTitle* self = (LSTitle*)self_;
    gtk_label_set_text(GTK_LABEL(self->title), game->title);
    sprintf(str, "#%d", game->attempt_count);
    gtk_label_set_text(GTK_LABEL(self->attempt_count), str);
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The Title component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void title_draw(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    char attempt_str[64];
    char finished_str[64];
    char combi_str[64];
    LSTitle* self = (LSTitle*)self_;
    sprintf(attempt_str, "%d", game->attempt_count);
    sprintf(finished_str, "#%d", game->finished_count);
    strcpy(combi_str, finished_str);
    strcat(combi_str, "/");
    strcat(combi_str, attempt_str);
    gtk_label_set_text(GTK_LABEL(self->attempt_count), combi_str);
}

LSComponentOps ls_title_operations = {
    .delete = title_delete,
    .widget = title_widget,
    .resize = title_resize,
    .show_game = title_show_game,
    .draw = title_draw
};
