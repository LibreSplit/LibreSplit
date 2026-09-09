#include "gtk/gtk.h"
#include "gui/component/components.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"

const abi_version_t abi_version = 1; // v0.1
const char plugin_name[] = "Test Plugin 5";
const char plugin_description[] = "Does something, creates a fake component with some text";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

typedef struct LSString {
    LSComponent base;
    GtkWidget* box;
    GtkWidget* text;
} LSString;

extern LSComponentOps ls_string_operations;

LSComponent* ls_component_string_new(void)
{
    LSString* self;
    self = malloc(sizeof(LSString));
    if (!self) {
        return NULL;
    }
    self->base.ops = &ls_string_operations;
    self->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    add_class(self->box, "footer");
    gtk_widget_show(self->box);
    self->text = gtk_label_new("This is a test string from a plugin");
    gtk_container_add(GTK_CONTAINER(self->box), self->text);
    gtk_widget_show(self->text);
    return (LSComponent*)self;
}

static void string_delete(LSComponent* self)
{
    free(self);
}

static GtkWidget* string_widget(LSComponent* self)
{
    return ((LSString*)self)->box;
}

LSComponentOps ls_string_operations = {
    .delete = string_delete,
    .widget = string_widget,
};

int plug_init(PlugAPI* api)
{
    api->register_component("string", ls_component_string_new);
    return 0;
}
