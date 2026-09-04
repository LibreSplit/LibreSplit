#include "src/gui/backends/x11.h"

#include <gdk/x11/gdkx.h>

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD 1

/**
 * Returns whether or not the current display is x11
 *
 * @return bool
 */
bool is_x11_display(void)
{
    GdkDisplay* display = gdk_display_get_default();
    return display != NULL && GDK_IS_X11_DISPLAY(display);
}

/**
 * Requests that the X11 window manager add or remove a window state
 *
 * @param display The GDK X11 display associated with the window
 * @param window The X11 window to update
 * @param state The name of the _NET_WM_STATE to update
 * @param add Whether to add or remove the state
 */
static void change_wm_state(GdkDisplay* display, Window window, const char* state, gboolean add)
{
    XClientMessageEvent event = { 0 };
    event.type = ClientMessage;
    event.window = window;
    event.message_type = gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_STATE");
    event.format = 32;
    event.data.l[0] = add ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
    event.data.l[1] = gdk_x11_get_xatom_by_name_for_display(display, state);
    event.data.l[2] = 0;
    event.data.l[3] = 1;
    event.data.l[4] = 0;

    XSendEvent(gdk_x11_display_get_xdisplay(display),
        gdk_x11_display_get_xrootwindow(display),
        False,
        SubstructureRedirectMask | SubstructureNotifyMask,
        (XEvent*)&event);
}

/**
 * Requests that an X11 window be kept above other windows
 *
 * Enabling the state also removes _NET_WM_STATE_BELOW. This function does
 * nothing when the window is not backed by a mapped X11 surface.
 *
 * @param window The GTK window to update
 * @param setting Whether the window should be kept above other windows
 */
void x11_set_keep_above(GtkWindow* window, gboolean setting)
{
    GdkDisplay* display;
    GdkSurface* surface;
    Window xwindow;

    g_return_if_fail(GTK_IS_WINDOW(window));

    display = gtk_widget_get_display(GTK_WIDGET(window));
    if (display == NULL || !GDK_IS_X11_DISPLAY(display)) {
        return;
    }

    surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (surface == NULL
        || !GDK_IS_X11_SURFACE(surface)
        || gdk_surface_is_destroyed(surface)
        || !gdk_surface_get_mapped(surface)) {
        return;
    }

    setting = setting != FALSE;
    xwindow = gdk_x11_surface_get_xid(surface);

    if (setting) {
        change_wm_state(display, xwindow, "_NET_WM_STATE_BELOW", FALSE);
    }

    change_wm_state(display, xwindow, "_NET_WM_STATE_ABOVE", setting);
}
