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
LSComponent* ls_component_title_new(void)
{
    LSTitle* self;
    GtkWidget* counts;

    self = malloc(sizeof(LSTitle));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_title_operations;

    self->header = gtk_center_box_new();
    gtk_center_box_set_shrink_center_last(GTK_CENTER_BOX(self->header), FALSE);
    add_class(self->header, "header");

    self->title = gtk_label_new(NULL);
    add_class(self->title, "title");
    gtk_label_set_justify(GTK_LABEL(self->title), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(self->title), TRUE);
    gtk_widget_set_hexpand(self->title, TRUE);
    gtk_center_box_set_center_widget(GTK_CENTER_BOX(self->header), self->title);

    counts = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    self->attempt_count = gtk_label_new(NULL);
    add_class(self->attempt_count, "attempt-count");
    gtk_widget_set_margin_start(self->attempt_count, 8);
    gtk_widget_set_valign(self->attempt_count, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(counts), self->attempt_count);

    self->finished_count = gtk_label_new(NULL);
    add_class(self->finished_count, "finished_count");
    gtk_widget_set_margin_start(self->finished_count, 8);
    gtk_widget_set_valign(self->finished_count, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(counts), self->finished_count);

    gtk_center_box_set_end_widget(GTK_CENTER_BOX(self->header), counts);

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
    .show_game = title_show_game,
    .draw = title_draw
};
