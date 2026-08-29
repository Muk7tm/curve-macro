#include "gui.h"

#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gboolean on_unix_signal(gpointer user_data)
{
    (void)user_data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    const char *session_type = getenv("XDG_SESSION_TYPE");
    const char *display = getenv("DISPLAY");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");

    if ((session_type && strcmp(session_type, "wayland") == 0) ||
        (wayland_display && *wayland_display && (!session_type || strcmp(session_type, "x11") != 0)))
    {
        fprintf(stderr, "Curve Macro targets X11 only. Log into a Plasma X11 session and try again.\n");
        return 1;
    }
    if (!display || !*display)
    {
        fprintf(stderr, "DISPLAY is not set. Curve Macro requires an X11 display.\n");
        return 1;
    }

    if (!XInitThreads())
    {
        fprintf(stderr, "Could not initialize Xlib threading support.\n");
        return 1;
    }

    gdk_set_allowed_backends("x11");
    g_unix_signal_add(SIGINT, on_unix_signal, NULL);
    g_unix_signal_add(SIGTERM, on_unix_signal, NULL);

    return gui_run(argc, argv);
}
