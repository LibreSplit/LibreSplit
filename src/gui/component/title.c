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
    GtkWidget* start; /*!< Container for the icon and left aligned title */
    GtkWidget* icon; /*!< The container for the icon */
    GtkWidget* title; /*!< The label containing the title itself */
    GtkWidget* attempt_count; /*!< The label containing the number of attempts. */
    GtkWidget* finished_count; /*<! The label containing the number of finished runs. */
    GtkCssProvider* icon_css_provider; /*<! CSS provider for displaying the game icon. */
} LSTitle;
extern LSComponentOps ls_title_operations; // defined at the end of the file

static const LSComponentSetting title_settings[] = {
    { .key = "show_game", .label = "Show Game Name", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "show_category", .label = "Show Category Name", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "single_line", .label = "Show Text on One Line", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "text_alignment", .label = "Text Alignment", .val = { .type = LS_COMPONENT_SETTING_INTEGER, .int_val = 2 } },
    { .key = "show_attempts", .label = "Show Attempts Count", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "show_finished", .label = "Show Finished Runs Count", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { .key = "show_icon", .label = "Show Game Icon", .val = { .type = LS_COMPONENT_SETTING_BOOLEAN, .bool_val = true } },
    { NULL }
};

/**
 * Constructor
 */
LSComponent* ls_component_title_new(void)
{
    LSTitle* self;
    GtkWidget* counts;

    self = calloc(1, sizeof(LSTitle));
    if (!self) {
        return NULL;
    }

    self->base.ops = &ls_title_operations;

    self->header = gtk_center_box_new();
    gtk_center_box_set_shrink_center_last(GTK_CENTER_BOX(self->header), FALSE);
    add_class(self->header, "header");

    self->start = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_center_box_set_start_widget(GTK_CENTER_BOX(self->header), self->start);

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
 * Clears the icon when the game changes.
 *
 * @param self_ The Title component itself.
 */
static void title_clear_game(LSComponent* self_)
{
    LSTitle* self = (LSTitle*)self_;
    if (self->icon_css_provider) {
        gtk_style_context_remove_provider_for_display(
            gtk_widget_get_display(self->header),
            GTK_STYLE_PROVIDER(self->icon_css_provider));
        g_clear_object(&self->icon_css_provider);
    }

    if (self->icon) {
        gtk_box_remove(GTK_BOX(self->start), self->icon);
        self->icon = NULL;
    }
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void title_delete(LSComponent* self)
{
    title_clear_game(self);
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
    LSTitle* self = (LSTitle*)self_;

    const char* game_name = game->name ? game->name : "";
    const char* category = game->category ? game->category : "";
    bool single_line = ls_component_settings_get(self_, "single_line")->bool_val;
    bool show_game = ls_component_settings_get(self_, "show_game")->bool_val;
    bool show_category = ls_component_settings_get(self_, "show_category")->bool_val;
    if (!show_game) {
        game_name = "";
    }

    if (!show_category) {
        category = "";
    }

    if (*game_name && *category) {
        char* title = g_strdup_printf("%s%c%s", game_name, single_line ? ' ' : '\n', category);
        gtk_label_set_text(GTK_LABEL(self->title), title);
        g_free(title);
    } else {
        gtk_label_set_text(GTK_LABEL(self->title), *game_name ? game_name : category);
    }

    gtk_widget_set_valign(self->attempt_count, single_line ? GTK_ALIGN_START : GTK_ALIGN_CENTER);
    gtk_widget_set_valign(self->finished_count, single_line ? GTK_ALIGN_START : GTK_ALIGN_CENTER);

    bool show_attempts = ls_component_settings_get(self_, "show_attempts")->bool_val;
    if (show_attempts) {
        char str[65];
        sprintf(str, "#%d", game->attempt_count);
        gtk_label_set_text(GTK_LABEL(self->attempt_count), str);
    }
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
    LSTitle* self = (LSTitle*)self_;

    bool show_attempts = ls_component_settings_get(self_, "show_attempts")->bool_val;
    bool show_finished = ls_component_settings_get(self_, "show_finished")->bool_val;
    bool show_icon = ls_component_settings_get(self_, "show_icon")->bool_val;
    gtk_widget_set_visible(gtk_widget_get_parent(self->attempt_count), show_attempts || show_finished);

    if (show_icon) {
        if (game->icon_path && !self->icon) {
            GString* icon_css_src = g_string_new(".title-icon { background-repeat: no-repeat; background-position: center; min-width: 20px; min-height: 20px; background-size: 20px; margin-right: 4px;");
            char* icon = icon_uri(game->path, game->icon_path);
            if (icon) {
                g_string_append(icon_css_src, " background-image: url(");
                append_quoted_uri(icon_css_src, icon);
                g_string_append(icon_css_src, ");");
                g_free(icon);
            }

            g_string_append(icon_css_src, " }");

            self->icon_css_provider = gtk_css_provider_new();
            gtk_css_provider_load_from_string(self->icon_css_provider, icon_css_src->str);
            gtk_style_context_add_provider_for_display(
                gtk_widget_get_display(self->header),
                GTK_STYLE_PROVIDER(self->icon_css_provider),
                GTK_STYLE_PROVIDER_PRIORITY_USER);
            g_string_free(icon_css_src, TRUE);

            self->icon = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            add_class(self->icon, "title-icon");
            gtk_box_prepend(GTK_BOX(self->start), self->icon);
        }
    } else if (self->icon) {
        title_clear_game(self_);
    }

    long alignment = ls_component_settings_get(self_, "text_alignment")->int_val;
    bool align_left = alignment == 1 || (alignment == 0 && self->icon);
    if (align_left && gtk_widget_get_parent(self->title) == self->header) {
        g_object_ref(self->title);
        gtk_center_box_set_center_widget(GTK_CENTER_BOX(self->header), NULL);
        gtk_box_append(GTK_BOX(self->start), self->title);
        g_object_unref(self->title);
    } else if (!align_left && gtk_widget_get_parent(self->title) == self->start) {
        g_object_ref(self->title);
        gtk_box_remove(GTK_BOX(self->start), self->title);
        gtk_center_box_set_center_widget(GTK_CENTER_BOX(self->header), self->title);
        g_object_unref(self->title);
    }

    gtk_label_set_xalign(GTK_LABEL(self->title), align_left ? 0.0f : 0.5f);
    gtk_label_set_justify(GTK_LABEL(self->title), align_left ? GTK_JUSTIFY_LEFT : GTK_JUSTIFY_CENTER);

    if (show_attempts && show_finished) {
        char attempt_str[64];
        char finished_str[65];
        char combi_str[130];
        sprintf(attempt_str, "%d", game->attempt_count);
        sprintf(finished_str, "#%d", game->finished_count);
        strcpy(combi_str, finished_str);
        strcat(combi_str, "/");
        strcat(combi_str, attempt_str);
        gtk_label_set_text(GTK_LABEL(self->attempt_count), combi_str);
    } else if (show_attempts) {
        char attempt_str[65];
        sprintf(attempt_str, "#%d", game->attempt_count);
        gtk_label_set_text(GTK_LABEL(self->attempt_count), attempt_str);
    } else if (show_finished) {
        char finished_str[65];
        sprintf(finished_str, "#%d", game->finished_count);
        gtk_label_set_text(GTK_LABEL(self->attempt_count), finished_str);
    }
}

LSComponentOps ls_title_operations = {
    .settings = title_settings,
    .delete = title_delete,
    .widget = title_widget,
    .show_game = title_show_game,
    .clear_game = title_clear_game,
    .draw = title_draw
};
