/* bind.c
 * Copyright (C) 2008 Alex Graveley
 * Copyright (C) 2010 Ulrik Sverdrup <ulrik.sverdrup@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "src/gui/widgets/dialog.h"
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

#include "bind.h"

/* Uncomment the next line to print a debug trace. */
/* #define DEBUG */

#ifdef DEBUG
#define TRACE(x) x
#else
#define TRACE(x) \
    do {         \
    } while (FALSE);
#endif

#define MODIFIERS_ERROR ((GdkModifierType)(-1))
#define MODIFIERS_NONE 0

/* Group to use: Which of configured keyboard Layouts
 * Since grabbing a key blocks its use, we can't grab the corresponding
 * (physical) keys for alternative layouts.
 *
 * Because of this, we interpret all keys relative to the default
 * keyboard layout.
 *
 * For example, if you bind "w", the physical W key will respond to
 * the bound key, even if you switch to a keyboard layout where the W key
 * types a different letter.
 */
#define WE_ONLY_USE_ONE_GROUP 0

struct Binding {
    KeybinderHandler handler;
    void* user_data;
    char* keystring;
    GDestroyNotify notify;
    /* GDK "distilled" values */
    guint keyval;
    GdkModifierType modifiers;
};

static GSList* bindings = NULL;
static guint32 last_event_time = 0;
static gboolean processing_event = FALSE;
static GdkModifierType modmap[8];
static int xkb_event_type = 0;
static GdkDisplay* keybinder_display = NULL;
static gulong xevent_handler_id = 0;

/* Build a map from X11's real modifier slots to the corresponding
 * Meta, Super, and Hyper modifiers. This replaces the modifier map
 * that GdkKeymap maintained internally in GTK 3.
 */
static void
update_modmap(Display* xdisplay)
{
    static struct {
        const char* name;
        Atom atom;
        GdkModifierType mask;
    } virtual_modifiers[] = {
        { "Meta", None, GDK_META_MASK },
        { "Super", None, GDK_SUPER_MASK },
        { "Hyper", None, GDK_HYPER_MASK },
    };
    XkbDescPtr xkb;
    guint i;
    guint j;
    guint k;

    for (i = 0; i < 8; i++) {
        modmap[i] = 1 << i;
    }

    xkb = XkbGetMap(xdisplay, XkbVirtualModsMask, XkbUseCoreKbd);
    if (xkb == NULL) {
        return;
    }

    if (XkbGetNames(xdisplay, XkbVirtualModNamesMask, xkb) != Success) {
        XkbFreeKeyboard(xkb, XkbAllComponentsMask, TRUE);
        return;
    }

    for (i = 0; i < G_N_ELEMENTS(virtual_modifiers); i++) {
        if (virtual_modifiers[i].atom == None) {
            virtual_modifiers[i].atom = XInternAtom(xdisplay,
                virtual_modifiers[i].name,
                False);
        }
    }

    for (i = 0; i < XkbNumVirtualMods; i++) {
        for (j = 0; j < G_N_ELEMENTS(virtual_modifiers); j++) {
            if (xkb->names->vmods[i] != virtual_modifiers[j].atom) {
                continue;
            }

            for (k = 0; k < 8; k++) {
                if (xkb->server->vmods[i] & (1 << k)) {
                    modmap[k] |= virtual_modifiers[j].mask;
                }
            }
        }
    }

    XkbFreeKeyboard(xkb, XkbAllComponentsMask, TRUE);
}

/* Add the X11 modifier bits represented by virtual GTK modifiers.
 */
static gboolean
map_virtual_modifiers(GdkModifierType* modifiers)
{
    const GdkModifierType virtual_modifiers[] = {
        GDK_SUPER_MASK,
        GDK_HYPER_MASK,
        GDK_META_MASK,
    };
    gboolean success = TRUE;
    guint i;
    guint j;

    for (j = 0; j < G_N_ELEMENTS(virtual_modifiers); j++) {
        if (*modifiers & virtual_modifiers[j]) {
            for (i = 4; i < 8; i++) {
                if (modmap[i] & virtual_modifiers[j]) {
                    if (*modifiers & (1 << i)) {
                        success = FALSE;
                    } else {
                        *modifiers |= 1 << i;
                    }
                }
            }
        }
    }

    return success;
}

/* Add the virtual GTK modifiers represented by X11 modifier bits.
 * This makes an incoming X event comparable with a GTK accelerator.
 */
static void
add_virtual_modifiers(GdkModifierType* modifiers)
{
    guint i;

    for (i = 4; i < 8; i++) {
        if (*modifiers & (1 << i)) {
            if (modmap[i] & GDK_SUPER_MASK) {
                *modifiers |= GDK_SUPER_MASK;
            }
            if (modmap[i] & GDK_HYPER_MASK) {
                *modifiers |= GDK_HYPER_MASK;
            }
            if (modmap[i] & GDK_META_MASK) {
                *modifiers |= GDK_META_MASK;
            }
        }
    }
}

/* Return the modifier mask that needs to be pressed to produce key in the
 * given group (keyboard layout) and level ("shift level").
 */
static GdkModifierType
FinallyGetModifiersForKeycode(XkbDescPtr xkb,
    KeyCode key,
    uint group,
    uint level)
{
    int nKeyGroups;
    int effectiveGroup;
    XkbKeyTypeRec* type;
    int k;

    nKeyGroups = XkbKeyNumGroups(xkb, key);
    if ((!XkbKeycodeInRange(xkb, key)) || (nKeyGroups == 0)) {
        return MODIFIERS_ERROR;
    }

    /* Taken from GDK's MyEnhancedXkbTranslateKeyCode */
    /* find the offset of the effective group */
    effectiveGroup = group;
    if (effectiveGroup >= nKeyGroups) {
        unsigned groupInfo = XkbKeyGroupInfo(xkb, key);
        switch (XkbOutOfRangeGroupAction(groupInfo)) {
            default:
                effectiveGroup %= nKeyGroups;
                break;
            case XkbClampIntoRange:
                effectiveGroup = nKeyGroups - 1;
                break;
            case XkbRedirectIntoRange:
                effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
                if (effectiveGroup >= nKeyGroups)
                    effectiveGroup = 0;
                break;
        }
    }
    type = XkbKeyKeyType(xkb, key, effectiveGroup);
    for (k = 0; k < type->map_count; k++) {
        if (type->map[k].active && type->map[k].level == level) {
            if (type->preserve) {
                return (type->map[k].mods.mask & ~type->preserve[k].mask);
            } else {
                return type->map[k].mods.mask;
            }
        }
    }
    return MODIFIERS_NONE;
}

/* Grab or ungrab the keycode+modifiers combination, first plainly, and then
 * including each ignorable modifier in turn.
 */
static gboolean
grab_ungrab_with_ignorable_modifiers(Window rootwin,
    uint keycode,
    uint modifiers,
    gboolean grab)
{
    guint i;
    gboolean success = FALSE;
    GdkDisplay* display = gdk_display_get_default();
    Display* xdisplay = gdk_x11_display_get_xdisplay(display);

    /* Ignorable modifiers */
    guint mod_masks[] = {
        0, /* modifier only */
        Mod2Mask,
        GDK_LOCK_MASK,
        Mod2Mask | GDK_LOCK_MASK,
    };

    gdk_x11_display_error_trap_push(display);

    for (i = 0; i < G_N_ELEMENTS(mod_masks); i++) {
        if (grab) {
            XGrabKey(xdisplay,
                keycode,
                modifiers | mod_masks[i],
                rootwin,
                True,
                GrabModeSync,
                GrabModeSync);
        } else {
            XUngrabKey(xdisplay,
                keycode,
                modifiers | mod_masks[i],
                rootwin);
        }
    }
    gdk_display_flush(display);
    if (gdk_x11_display_error_trap_pop(display)) {
        TRACE(g_warning("Failed grab/ungrab"));
        if (grab) {
            /* On error, immediately release keys again */
            grab_ungrab_with_ignorable_modifiers(rootwin,
                keycode,
                modifiers,
                FALSE);
        }
    } else {
        success = TRUE;
    }
    return success;
}

/* Grab or ungrab then keyval and modifiers combination, grabbing all key
 * combinations yielding the same key values.
 * Includes ignorable modifiers using grab_ungrab_with_ignorable_modifiers.
 */
static gboolean
grab_ungrab(Window rootwin,
    uint keyval,
    uint modifiers,
    gboolean grab)
{
    int k;
    GdkKeymapKey* keys = NULL;
    gint n_keys = 0;
    GdkModifierType add_modifiers;
    XkbDescPtr xmap;
    gboolean success = FALSE;
    GdkDisplay* display = gdk_display_get_default();
    Display* xdisplay = gdk_x11_display_get_xdisplay(display);

    xmap = XkbGetMap(xdisplay,
        XkbAllClientInfoMask,
        XkbUseCoreKbd);

    if (xmap == NULL) {
        return FALSE;
    }

    if (!gdk_display_map_keyval(display, keyval, &keys, &n_keys) || n_keys == 0) {
        g_free(keys);
        XkbFreeKeyboard(xmap, XkbAllComponentsMask, TRUE);
        return FALSE;
    }

    for (k = 0; k < n_keys; k++) {
        /* NOTE: We only bind for the first group,
         * so regardless of current keyboard layout, it will
         * grab the key from the default Layout.
         */
        if (keys[k].group != WE_ONLY_USE_ONE_GROUP) {
            continue;
        }

        add_modifiers = FinallyGetModifiersForKeycode(xmap,
            keys[k].keycode,
            keys[k].group,
            keys[k].level);

        if (add_modifiers == MODIFIERS_ERROR) {
            continue;
        }
        TRACE(g_print("grab/ungrab keycode: %d, lev: %d, grp: %d, ",
            keys[k].keycode, keys[k].level, keys[k].group));
        TRACE(g_print("modifiers: 0x%x (consumed: 0x%x)\n",
            add_modifiers | modifiers, add_modifiers));
        if (grab_ungrab_with_ignorable_modifiers(rootwin,
                keys[k].keycode,
                add_modifiers | modifiers,
                grab)) {

            success = TRUE;
        } else {
            /* When grabbing, break on error */
            if (grab && !success) {
                break;
            }
        }
    }
    g_free(keys);
    XkbFreeKeyboard(xmap, XkbAllComponentsMask, TRUE);

    return success;
}

static gboolean
keyvalues_equal(guint kv1, guint kv2)
{
    return kv1 == kv2;
}

/* Compare modifier set equality,
 * while accepting overloaded modifiers (MOD1 and META together)
 */
static gboolean
modifiers_equal(GdkModifierType mf1, GdkModifierType mf2)
{
    GdkModifierType ignored = 0;

    /* Accept MOD1 + META as MOD1 */
    if (mf1 & mf2 & GDK_ALT_MASK) {
        ignored |= GDK_META_MASK;
    }
    /* Accept SUPER + HYPER as SUPER */
    if (mf1 & mf2 & GDK_SUPER_MASK) {
        ignored |= GDK_HYPER_MASK;
    }
    if ((mf1 & ~ignored) == (mf2 & ~ignored)) {
        return TRUE;
    }
    return FALSE;
}

static gboolean
do_grab_key(struct Binding* binding)
{
    gboolean success;
    GdkDisplay* display = gdk_display_get_default();
    Window rootwin = gdk_x11_display_get_xrootwindow(display);

    GdkModifierType modifiers;
    guint keysym = 0;

    if (rootwin == None) {
        return FALSE;
    }

    gtk_accelerator_parse(binding->keystring, &keysym, &modifiers);

    if (keysym == 0) {
        return FALSE;
    }

    binding->keyval = keysym;
    binding->modifiers = modifiers;
    TRACE(g_print("Grabbing keyval: %u, vmodifiers: 0x%x, name: %s\n",
        keysym, modifiers, binding->keystring));

    /* Map virtual modifiers to non-virtual modifiers */
    map_virtual_modifiers(&modifiers);

    if (modifiers == binding->modifiers && (GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK) & modifiers) {
        g_warning("Failed to map virtual modifiers");
        return FALSE;
    }

    success = grab_ungrab(rootwin, keysym, modifiers, TRUE /* grab */);

    if (!success) {
        g_warning("Binding '%s' failed!", binding->keystring);
    }

    return success;
}

static gboolean
do_ungrab_key(struct Binding* binding)
{
    GdkDisplay* display = gdk_display_get_default();
    Window rootwin = gdk_x11_display_get_xrootwindow(display);
    GdkModifierType modifiers;

    if (rootwin == None) {
        return FALSE;
    }

    TRACE(g_print("Ungrabbing keyval: %u, vmodifiers: 0x%x, name: %s\n",
        binding->keyval, binding->modifiers, binding->keystring));

    /* Map virtual modifiers to non-virtual modifiers */
    modifiers = binding->modifiers;
    map_virtual_modifiers(&modifiers);

    grab_ungrab(rootwin, binding->keyval, modifiers, FALSE /* ungrab */);
    return TRUE;
}

static void keymap_changed(Display* xdisplay);

static gboolean
filter_func(GdkDisplay* display, gpointer gdk_xevent, gpointer data)
{
    XEvent* xevent = (XEvent*)gdk_xevent;
    Display* xdisplay = gdk_x11_display_get_xdisplay(display);
    Window rootwin = GDK_POINTER_TO_XID(data);
    guint keyval;
    GdkModifierType consumed, modifiers;
    guint mod_mask = gtk_accelerator_get_default_mod_mask();
    GSList* iter;

    if (xevent->type == MappingNotify || (xkb_event_type != 0 && xevent->type == xkb_event_type && (((XkbEvent*)xevent)->any.xkb_type == XkbNewKeyboardNotify || ((XkbEvent*)xevent)->any.xkb_type == XkbMapNotify))) {
        keymap_changed(xdisplay);
        return FALSE;
    }

    if (xevent->xany.window != rootwin) {
        return FALSE;
    }

    switch (xevent->type) {
        case KeyPress:
            modifiers = xevent->xkey.state;

            TRACE(g_print("Got KeyPress keycode: %d, modifiers: 0x%x\n",
                xevent->xkey.keycode,
                xevent->xkey.state));

            gdk_display_translate_key(
                display,
                xevent->xkey.keycode,
                modifiers,
                /* See top comment why we don't use this here:
                XkbGroupForCoreState (xevent->xkey.state)
                */
                WE_ONLY_USE_ONE_GROUP,
                &keyval, NULL, NULL, &consumed);

            /* Map non-virtual to virtual modifiers */
            modifiers &= ~consumed;
            add_virtual_modifiers(&modifiers);
            modifiers &= mod_mask;

#ifdef DEBUG
            {
                gchar* accelerator = gtk_accelerator_name(keyval, modifiers);
                TRACE(g_print("Translated keyval: %u, vmodifiers: 0x%x, name: %s\n",
                    keyval, modifiers, accelerator));
                g_free(accelerator);
            }
#endif

            /*
             * Set the last event time for use when showing
             * windows to avoid anti-focus-stealing code.
             */
            processing_event = TRUE;
            last_event_time = xevent->xkey.time;

            // Don't allow global hotkeys to affect the timer while a dialog is presented to the user
            if (!ls_dialog_exists()) {
                iter = bindings;
                while (iter != NULL) {
                    /* NOTE: ``iter`` might be removed from the list
                     * in the callback.
                     */
                    struct Binding* binding = iter->data;
                    iter = iter->next;

                    if (keyvalues_equal(binding->keyval, keyval) && modifiers_equal(binding->modifiers, modifiers)) {
                        TRACE(g_print("Calling handler for '%s'...\n",
                            binding->keystring));

                        (binding->handler)(binding->keystring, binding->user_data);
                        // in case a dialog is opened by this binding, stop processing global hotkeys for now
                        if (ls_dialog_exists()) {
                            break;
                        }
                    }
                }
            }

            processing_event = FALSE;
            break;
        case KeyRelease:
            TRACE(g_print("Got KeyRelease! \n"));
            break;
    }
    XAllowEvents(xdisplay, ReplayKeyboard, xevent->xkey.time);
    XFlush(xdisplay);

    return FALSE;
}

static void
keymap_changed(Display* xdisplay)
{
    GSList* iter;

    TRACE(g_print("Keymap changed! Regrabbing keys..."));

    update_modmap(xdisplay);

    for (iter = bindings; iter != NULL; iter = iter->next) {
        struct Binding* binding = iter->data;
        do_ungrab_key(binding);
    }

    for (iter = bindings; iter != NULL; iter = iter->next) {
        struct Binding* binding = iter->data;
        do_grab_key(binding);
    }
}

/**
 * keybinder_init:
 *
 * Initialize the keybinder library.
 *
 * This function must be called after initializing GTK, before calling any
 * other function in the library. Can only be called once.
 */
void keybinder_init(void)
{
    GdkDisplay* display = gdk_display_get_default();
    if (keybinder_display != NULL || display == NULL) {
        return;
    }

    Display* xdisplay = gdk_x11_display_get_xdisplay(display);
    Window rootwin = gdk_x11_display_get_xrootwindow(display);
    int xkb_opcode;
    int xkb_error_type;
    int xkb_major = XkbMajorVersion;
    int xkb_minor = XkbMinorVersion;

    XkbQueryExtension(xdisplay,
        &xkb_opcode,
        &xkb_event_type,
        &xkb_error_type,
        &xkb_major,
        &xkb_minor);
    update_modmap(xdisplay);

    keybinder_display = g_object_ref(display);
    xevent_handler_id = g_signal_connect_after(display,
        "xevent",
        G_CALLBACK(filter_func),
        GDK_XID_TO_POINTER(rootwin));
}

/**
 * @brief Release global key grabs and disconnect X11 event handler.
 */
void keybinder_dispose(void)
{
    if (keybinder_display != NULL && xevent_handler_id != 0) {
        g_signal_handler_disconnect(keybinder_display, xevent_handler_id);
        xevent_handler_id = 0;
    }

    GList* old_bindings = g_steal_pointer(&bindings);
    for (GSList* iter = old_bindings; iter != NULL; iter = iter->next) {
        struct Binding* binding = iter->data;
        do_ungrab_key(binding);
        if (binding->notify) {
            binding->notify(binding->user_data);
        }

        g_free(binding->keystring);
        g_free(binding);
    }

    g_list_free(old_bindings);
    g_clear_object(&keybinder_display);
    processing_event = FALSE;
    last_event_time = 0;
    xkb_event_type = 0;
}

/**
 * keybinder_bind: (skip)
 * @keystring: an accelerator description (gtk_accelerator_parse() format)
 * @handler:   callback function
 * @user_data: data to pass to @handler
 *
 * Grab a key combination globally and register a callback to be called each
 * time the key combination is pressed.
 *
 * This function is excluded from introspected bindings and is replaced by
 * keybinder_bind_full.
 *
 * Returns: %TRUE if the accelerator could be grabbed
 */
gboolean
keybinder_bind(const char* keystring,
    KeybinderHandler handler,
    void* user_data)
{
    return keybinder_bind_full(keystring, handler, user_data, NULL);
}

/**
 * keybinder_bind_full:
 * @keystring: an accelerator description (gtk_accelerator_parse() format)
 * @handler:   (scope notified):        callback function
 * @user_data: (closure) (allow-none):  data to pass to @handler
 * @notify:    (allow-none):  called when @handler is unregistered
 *
 * Grab a key combination globally and register a callback to be called each
 * time the key combination is pressed.
 *
 * Rename to: keybinder_bind
 *
 * Since: 0.3.0
 *
 * Returns: %TRUE if the accelerator could be grabbed
 */
gboolean
keybinder_bind_full(const char* keystring,
    KeybinderHandler handler,
    void* user_data,
    GDestroyNotify notify)
{
    struct Binding* binding;
    gboolean success;

    binding = g_new0(struct Binding, 1);
    binding->keystring = g_strdup(keystring);
    binding->handler = handler;
    binding->user_data = user_data;
    binding->notify = notify;

    /* Sets the binding's keycode and modifiers */
    success = do_grab_key(binding);

    if (success) {
        bindings = g_slist_prepend(bindings, binding);
    } else {
        g_free(binding->keystring);
        g_free(binding);
    }
    return success;
}

/**
 * keybinder_unbind: (skip)
 * @keystring: an accelerator description (gtk_accelerator_parse() format)
 * @handler:   callback function
 *
 * Unregister a specific previously bound callback for this keystring.
 *
 * This function is excluded from introspected bindings and is replaced by
 * keybinder_unbind_all.
 */
void keybinder_unbind(const char* keystring, KeybinderHandler handler)
{

    for (GSList* iter = bindings; iter != NULL; iter = iter->next) {
        struct Binding* binding = iter->data;

        if (strcmp(keystring, binding->keystring) != 0 || handler != binding->handler)
            continue;

        do_ungrab_key(binding);
        bindings = g_slist_remove(bindings, binding);

        TRACE(g_print("unbind, notify: 0x%" PRIxPTR "\n", (uintptr_t)binding->notify));
        if (binding->notify) {
            binding->notify(binding->user_data);
        }
        g_free(binding->keystring);
        g_free(binding);
        break;
    }
}

/**
 * keybinder_unbind_all:
 * @keystring: an accelerator description (gtk_accelerator_parse() format)
 *
 * Unregister all previously bound callbacks for this keystring.
 *
 * Rename to: keybinder_unbind
 *
 * Since: 0.3.0
 */
void keybinder_unbind_all(const char* keystring)
{

    for (GSList* iter = bindings; iter != NULL; iter = iter->next) {
        struct Binding* binding = iter->data;

        if (strcmp(keystring, binding->keystring) != 0) {
            continue;
        }

        do_ungrab_key(binding);
        bindings = g_slist_remove(bindings, binding);

        TRACE(g_print("unbind_all, notify: 0x%" PRIxPTR "\n", (uintptr_t)binding->notify));
        if (binding->notify) {
            binding->notify(binding->user_data);
        }
        g_free(binding->keystring);
        g_free(binding);

        /* re-start scan from head of new list */
        iter = bindings;
        if (!iter)
            break;
    }
}

/**
 * keybinder_get_current_event_time:
 *
 * Returns: the current event timestamp
 */
guint32
keybinder_get_current_event_time(void)
{
    if (processing_event) {
        return last_event_time;
    } else {
        return GDK_CURRENT_TIME;
    }
}
