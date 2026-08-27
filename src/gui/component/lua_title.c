/** \file lua_title.c
 *
 * Implementation of the lua title component
 */
#include "../../lasr/export.h"
#include "../../lasr/utils.h"
#include "components.h"

/**
 * @brief The component representing the title.
 *
 * Represents the title of the run
 */
typedef struct _LSLuaTitle {
    LSComponent base; /*!< The base struct that is extended */
    GtkWidget* header; /*!< The container for the title */
    GtkWidget* title; /*!< The label containing the title itself */
    lasr_global* contents;
} LSLuaTitle;
extern LSComponentOps ls_lua_title_operations; // defined at the end of the file

/**
 * Constructor
 */
LSComponent* ls_component_lua_title_new(void)
{
    LSLuaTitle* self;

    self = malloc(sizeof(LSLuaTitle));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_lua_title_operations;

    /* NOTE: This implementation is mostly a stubbed example. Once configurable
     * splitter components are introduced, this variable name can be derived
     * from that, allowing the user to specify a value that is meaningful. */
    self->contents = lasr_global_create("dummyLuaTitleVar");
    register_shared_global(self->contents);
    /* *** */

    self->header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->header, "header");
    gtk_widget_show(self->header);

    self->title = gtk_label_new(NULL);
    add_class(self->title, "title");
    gtk_label_set_justify(GTK_LABEL(self->title), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(self->title), TRUE);
    gtk_widget_set_hexpand(self->title, TRUE);
    gtk_container_add(GTK_CONTAINER(self->header), self->title);

    return (LSComponent*)self;
}

/**
 * Destructor
 *
 * @param self The component to destroy
 */
static void lua_title_delete(LSComponent* self_)
{
    LSLuaTitle* self = (LSLuaTitle*)self_;
    lasr_global_release(self->contents);
    free(self_);
}

/**
 * Returns the Title GTK widget.
 *
 * @param self The Title component itself.
 * @return The container as a GTK Widget.
 */
static GtkWidget* lua_title_widget(LSComponent* self)
{
    return ((LSLuaTitle*)self)->header;
}

/**
 * Function to execute when resize_window is executed (the LibreSplit window is resized).
 *
 * @param self_ The title component itself.
 * @param win_width The new window width.
 * @param win_height The new window height.
 */
static void lua_title_resize(LSComponent* self_, int win_width, int win_height)
{
    GdkRectangle rect;
    LSLuaTitle* self = (LSLuaTitle*)self_;

    gtk_widget_hide(self->title);
    rect.width = win_width;
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
static void lua_title_show_game(LSComponent* self_, const ls_game* game,
    const ls_timer* timer)
{
    LSLuaTitle* self = (LSLuaTitle*)self_;
    gtk_label_set_text(GTK_LABEL(self->title), "(null)");
}

/**
 * Function to execute when ls_app_window_draw is executed.
 *
 * @param self_ The Title component itself.
 * @param game The game struct instance.
 * @param timer The timer instance.
 */
static void lua_title_draw(LSComponent* self_, const ls_game* game, const ls_timer* timer)
{
    LSLuaTitle* self = (LSLuaTitle*)self_;

    lasr_export title_ = { 0 };
    int type = import_shared_global(self->contents, &title_);
    char buf[32];

    if (type == LASR_TYPE_DYNAMIC) {
        gtk_label_set_text(GTK_LABEL(self->title), title_.dynamic->bytes);
    } else if (type == LASR_TYPE_ATOMIC) {
        /* Numeric type */
        snprintf(buf, sizeof(buf), "%d", title_.fixed);
        gtk_label_set_text(GTK_LABEL(self->title), buf);
    }

    /* Cleanup -- no memory was allocated if string not changed */
    lasr_export_resize(&title_, 0);
}

LSComponentOps ls_lua_title_operations = {
    .delete = lua_title_delete,
    .widget = lua_title_widget,
    .resize = lua_title_resize,
    .show_game = lua_title_show_game,
    .draw = lua_title_draw
};
