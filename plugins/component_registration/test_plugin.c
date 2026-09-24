#include "gtk/gtk.h"
#include "gui/component/components.h"
#include "plugins/plugin.h"
#include "plugins/plugin_utils.h"

/* =======================================
 * Metadata section
 * ---------------------------------------
 * Here we write all the metadata of the plugin, as well as
 * the supported ABI version
 */
const abi_version_t abi_version = 1; // v0.1
const char plugin_name[] = "Test Plugin 5";
const char plugin_description[] = "Does something, creates a fake component with some text";
const char plugin_version[] = "0.1";
const char plugin_author[] = "The LibreSplit Core Team";

// We define the component's struct, similarly to what happens in src/gui/component
typedef struct LSString {
    LSComponent base;
    GtkWidget* box;
    GtkWidget* text;
} LSString;

extern LSComponentOps ls_string_operations;

// Constructor function
LSComponent* ls_component_string_new(void)
{
    LSString* self;
    // Allocate memory for the component
    self = malloc(sizeof(LSString));
    if (!self) {
        // If the malloc fails, we just give up
        return NULL;
    }
    // Here we follow the construction of all the other components...
    self->base.ops = &ls_string_operations;
    // A box...
    self->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    // A label...
    self->text = gtk_label_new("This is a test string from a plugin");
    // Put the label in the box ...
    gtk_box_append(GTK_BOX(self->box), self->text);
    // We're done.
    return (LSComponent*)self;
}

// Destructor Function
static void string_delete(LSComponent* self)
{
    free(self);
}

// Get the GTK Widget from our struct
static GtkWidget* string_widget(LSComponent* self)
{
    return ((LSString*)self)->box;
}

// Operations that are used internally by Libresplit are now wired here.
LSComponentOps ls_string_operations = {
    .delete = string_delete,
    .widget = string_widget,
};

// Plugin initialization function
int plug_init(PlugAPI* api)
{
    // We register a component named "string" which will be created using the
    // ls_component_string_new constructor.
    api->register_component("string", ls_component_string_new);
    // All done
    return 0;
}
