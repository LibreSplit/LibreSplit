#include "timer.h"
#include <limits.h>

typedef struct {
    GtkBox parent;
    bool gradient;
} LSTimerBox;

typedef GtkBoxClass LSTimerBoxClass;
G_DEFINE_TYPE(LSTimerBox, ls_timer_box, GTK_TYPE_BOX)

const LSComponentOption ls_timer_timing_method_options[] = {
    { .key = LS_COMPONENT_COMPARISON_DEFAULT, .label = "Default" },
    { .key = LS_COMPONENT_COMPARISON_REAL_TIME, .label = "Real Time" },
    { .key = LS_COMPONENT_COMPARISON_GAME_TIME, .label = "Game Time" },
    { 0 }
};

const LSComponentOption ls_timer_digits_format_options[] = {
    { .key = LS_TIMER_DIGITS_SINGLE_SECONDS, .label = "1" },
    { .key = LS_TIMER_DIGITS_DOUBLE_SECONDS, .label = "01" },
    { .key = LS_TIMER_DIGITS_SINGLE_MINUTES, .label = "0:01" },
    { .key = LS_TIMER_DIGITS_DOUBLE_MINUTES, .label = "00:01" },
    { .key = LS_TIMER_DIGITS_SINGLE_HOURS, .label = "0:00:01" },
    { .key = LS_TIMER_DIGITS_DOUBLE_HOURS, .label = "00:00:01" },
    { 0 }
};

const LSComponentOption ls_timer_accuracy_options[] = {
    { .key = LS_TIME_ACCURACY_DEFAULT, .label = "Default" },
    { .key = LS_TIME_ACCURACY_SECONDS, .label = "Seconds" },
    { .key = LS_TIME_ACCURACY_TENTHS, .label = "Tenths" },
    { .key = LS_TIME_ACCURACY_HUNDREDTHS, .label = "Hundredths" },
    { .key = LS_TIME_ACCURACY_MILLISECONDS, .label = "Milliseconds" },
    { 0 }
};

ls_time_method ls_component_timer_method(LSComponent* component, ls_time_method default_method)
{
    switch (ls_component_settings_get(component, "timing_method")->int_val) {
        case LS_COMPONENT_COMPARISON_DEFAULT:
            return default_method;
        case LS_COMPONENT_COMPARISON_REAL_TIME:
            return LS_REAL_TIME;
        case LS_COMPONENT_COMPARISON_GAME_TIME:
            return LS_GAME_TIME;
    }

    return default_method;
}

static void set_state_class(GtkWidget* widget, const char* name, bool enabled)
{
    if (enabled) {
        gtk_widget_add_css_class(widget, name);
    } else {
        gtk_widget_remove_css_class(widget, name);
    }
}

void ls_timer_set_style(GtkWidget* widget, ls_timer_status status)
{
    set_state_class(widget, "delay", status == LS_TIMER_IDLE);
    set_state_class(widget, "paused", status == LS_TIMER_PAUSED);
    set_state_class(widget, "behind", status == LS_TIMER_BEHIND_GAINING || status == LS_TIMER_BEHIND_LOSING);
    set_state_class(widget, "losing", status == LS_TIMER_AHEAD_LOSING || status == LS_TIMER_BEHIND_LOSING);
    set_state_class(widget, "best-split", status == LS_TIMER_BEST);
}

void ls_timer_string(char* string, char* fraction, long long time, LSTimerDigitsFormat digits_format, LSTimeAccuracy accuracy)
{
    fraction[0] = '\0';
    if (time == LLONG_MAX) {
        strcpy(string, "-");
        return;
    }

    const char* sign = time < 0 ? "-" : "";
    unsigned long long magnitude = time < 0 ? 0ULL - (unsigned long long)time : (unsigned long long)time;
    unsigned long long hours = magnitude / 3600000000ULL;
    unsigned int minutes = (magnitude / 60000000ULL) % 60;
    unsigned int seconds = (magnitude / 1000000ULL) % 60;

    if (hours || digits_format >= LS_TIMER_DIGITS_SINGLE_HOURS) {
        sprintf(string, "%s%0*llu:%02u:%02u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_HOURS ? 2 : 1, hours, minutes, seconds);
    } else if (minutes || digits_format >= LS_TIMER_DIGITS_SINGLE_MINUTES) {
        sprintf(string, "%s%0*u:%02u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_MINUTES ? 2 : 1, minutes, seconds);
    } else {
        sprintf(string, "%s%0*u", sign, digits_format == LS_TIMER_DIGITS_DOUBLE_SECONDS ? 2 : 1, seconds);
    }

    int decimals = ls_component_time_accuracy_decimals(accuracy);
    if (decimals > 0) {
        char subseconds[7];
        sprintf(subseconds, "%06llu", magnitude % 1000000ULL);
        sprintf(fraction, ".%.*s", decimals, subseconds);
    }
}

/**
 * Applies a light-to-dark gradient using the text node as an alpha mask.
 *
 * @param GskRenderNode* Pointer to the node to apply gradient to.
 * @return A new render node, owned by the caller.
 */
static GskRenderNode* gradient_text(GskRenderNode* node)
{
    const GdkRGBA* color = gsk_text_node_get_color(node);
    float hue, saturation, value;
    gtk_rgb_to_hsv(color->red, color->green, color->blue, &hue, &saturation, &value);
    GskColorStop stops[] = {
        { .offset = 0, .color = { .alpha = 1 } },
        { .offset = 1, .color = { color->red * 0.8f, color->green * 0.8f, color->blue * 0.8f, 1 } },
    };

    gtk_hsv_to_rgb(hue, saturation * 0.5f, MIN(1.0f, value * 1.5f + 0.1f),
        &stops[0].color.red, &stops[0].color.green, &stops[0].color.blue);

    PangoFontMetrics* metrics = pango_font_get_metrics(gsk_text_node_get_font(node), NULL);
    graphene_point_t top = *gsk_text_node_get_offset(node);
    graphene_point_t bottom = top;
    top.y -= pango_font_metrics_get_ascent(metrics) / (float)PANGO_SCALE;
    bottom.y += pango_font_metrics_get_descent(metrics) / (float)PANGO_SCALE;
    pango_font_metrics_unref(metrics);
    graphene_rect_t bounds;
    gsk_render_node_get_bounds(node, &bounds);

    GskRenderNode* fill = gsk_linear_gradient_node_new(&bounds, &top, &bottom, stops, G_N_ELEMENTS(stops));
    GskRenderNode* result = gsk_mask_node_new(fill, node, GSK_MASK_MODE_ALPHA);
    gsk_render_node_unref(fill);
    return result;
}

/**
 * Shades foreground text while preserving shadows and CSS effects.
 * Visits children backwards because GTK paints shadows before foreground text.
 *
 * @param node The original render node.
 * @param glyphs Remaining foreground glyphs to shade.
 * @return A render node reference, owned by the caller.
 */
static GskRenderNode* gradient_node(GskRenderNode* node, guint* glyphs)
{
    if (!*glyphs) {
        return gsk_render_node_ref(node);
    }

    GskRenderNode* child;
    GskRenderNode* result;
    switch (gsk_render_node_get_node_type(node)) {
        case GSK_TEXT_NODE:
            *glyphs -= MIN(*glyphs, gsk_text_node_get_num_glyphs(node));
            return gradient_text(node);
        case GSK_CONTAINER_NODE:
            {
                guint count = gsk_container_node_get_n_children(node);
                GskRenderNode** children = g_new(GskRenderNode*, count);
                for (guint i = count; i > 0; --i) {
                    children[i - 1] = gradient_node(gsk_container_node_get_child(node, i - 1), glyphs);
                }

                result = gsk_container_node_new(children, count);
                for (guint i = 0; i < count; ++i) {
                    gsk_render_node_unref(children[i]);
                }

                g_free(children);
                return result;
            }
        case GSK_TRANSFORM_NODE:
            child = gradient_node(gsk_transform_node_get_child(node), glyphs);
            result = gsk_transform_node_new(child, gsk_transform_node_get_transform(node));
            break;
        case GSK_SHADOW_NODE:
            {
                gsize count = gsk_shadow_node_get_n_shadows(node);
                GskShadow* shadows = g_new(GskShadow, count);
                for (gsize i = 0; i < count; ++i) {
                    shadows[i] = *gsk_shadow_node_get_shadow(node, i);
                }

                child = gradient_node(gsk_shadow_node_get_child(node), glyphs);
                result = gsk_shadow_node_new(child, shadows, count);
                g_free(shadows);
                break;
            }
        case GSK_CLIP_NODE:
            child = gradient_node(gsk_clip_node_get_child(node), glyphs);
            result = gsk_clip_node_new(child, gsk_clip_node_get_clip(node));
            break;
        case GSK_ROUNDED_CLIP_NODE:
            child = gradient_node(gsk_rounded_clip_node_get_child(node), glyphs);
            result = gsk_rounded_clip_node_new(child, gsk_rounded_clip_node_get_clip(node));
            break;
        case GSK_OPACITY_NODE:
            child = gradient_node(gsk_opacity_node_get_child(node), glyphs);
            result = gsk_opacity_node_new(child, gsk_opacity_node_get_opacity(node));
            break;
        case GSK_COLOR_MATRIX_NODE:
            child = gradient_node(gsk_color_matrix_node_get_child(node), glyphs);
            result = gsk_color_matrix_node_new(child, gsk_color_matrix_node_get_color_matrix(node), gsk_color_matrix_node_get_color_offset(node));
            break;
        case GSK_BLUR_NODE:
            child = gradient_node(gsk_blur_node_get_child(node), glyphs);
            result = gsk_blur_node_new(child, gsk_blur_node_get_radius(node));
            break;
        default:
            return gsk_render_node_ref(node);
    }

    gsk_render_node_unref(child);
    return result;
}

static guint layout_glyph_count(PangoLayout* layout)
{
    guint count = 0;
    for (GSList* line = pango_layout_get_lines_readonly(layout); line; line = line->next) {
        for (GSList* run = ((PangoLayoutLine*)line->data)->runs; run; run = run->next) {
            PangoGlyphString* glyphs = ((PangoGlyphItem*)run->data)->glyphs;
            for (int i = 0; i < glyphs->num_glyphs; ++i) {
                count += glyphs->glyphs[i].glyph != PANGO_GLYPH_EMPTY;
            }
        }
    }

    return count;
}

static void ls_timer_box_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    if (!((LSTimerBox*)widget)->gradient) {
        GTK_WIDGET_CLASS(ls_timer_box_parent_class)->snapshot(widget, snapshot);
        return;
    }

    for (GtkWidget* label = gtk_widget_get_first_child(widget); label; label = gtk_widget_get_next_sibling(label)) {
        if (!GTK_IS_LABEL(label)) {
            gtk_widget_snapshot_child(widget, label, snapshot);
            continue;
        }

        GtkSnapshot* original = gtk_snapshot_new();
        gtk_widget_snapshot_child(widget, label, original);
        GskRenderNode* node = gtk_snapshot_free_to_node(original);
        if (node) {
            guint glyphs = layout_glyph_count(gtk_label_get_layout(GTK_LABEL(label)));
            GskRenderNode* gradient = gradient_node(node, &glyphs);
            gtk_snapshot_append_node(snapshot, gradient);
            gsk_render_node_unref(gradient);
            gsk_render_node_unref(node);
        }
    }
}

static void ls_timer_box_class_init(LSTimerBoxClass* class)
{
    GTK_WIDGET_CLASS(class)->snapshot = ls_timer_box_snapshot;
    gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(class), "box");
}

/**
 * @brief Empty initializer required by G_DEFINE_TYPE
 *
 * @param self Unused
 */
static void ls_timer_box_init(LSTimerBox* self)
{
}

GtkWidget* ls_timer_box_new(void)
{
    return g_object_new(ls_timer_box_get_type(), "orientation", GTK_ORIENTATION_HORIZONTAL, NULL);
}

void ls_timer_set_gradient(GtkWidget* box, bool enabled)
{
    LSTimerBox* self = (LSTimerBox*)box;
    if (self->gradient != enabled) {
        self->gradient = enabled;
        gtk_widget_queue_draw(box);
    }
}

static void set_scale_attributes(GtkLabel* label, PangoAttrList* attributes)
{
    PangoAttrList* current = gtk_label_get_attributes(label);
    if (current == attributes || (current && attributes && pango_attr_list_equal(current, attributes))) {
        return;
    }

    gtk_label_set_attributes(label, attributes);
}

void ls_timer_set_text(GtkWidget* seconds, GtkWidget* fraction, const char* text, const char* fraction_text)
{
    PangoAttrList* attributes = NULL;
    size_t length = strlen(text);
    if (length > 5) {
        // Keep longer displays close to the width of MM:SS.
        attributes = pango_attr_list_new();
        pango_attr_list_insert(attributes, pango_attr_scale_new(5.0 / length));
    }

    set_scale_attributes(GTK_LABEL(seconds), attributes);
    set_scale_attributes(GTK_LABEL(fraction), attributes);
    if (attributes) {
        pango_attr_list_unref(attributes);
    }

    gtk_label_set_text(GTK_LABEL(seconds), text);
    gtk_label_set_text(GTK_LABEL(fraction), fraction_text);
}
