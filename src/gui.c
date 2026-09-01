#include "gui.h"

#include "camera.h"
#include "config.h"
#include "input.h"
#include "performance.h"

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct GuiApp
{
    GtkWidget *window;
    GtkWidget *status_label;
    GtkWidget *detected_label;
    GtkWidget *x11_label;
    GtkWidget *xtest_label;
    GtkWidget *evdev_label;
    GtkWidget *uinput_label;
    GtkWidget *keyboard_diag_label;
    GtkWidget *macro_diag_label;
    GtkWidget *calibration_spin;
    GtkWidget *steps_spin;
    GtkWidget *step_delay_spin;
    GtkWidget *click_switch;
    GtkWidget *click_delay_spin;
    GtkWidget *device_combo;
    GtkWidget *key_label;
    GtkWidget *record_key_button;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    AppConfig config;
    InputDevice devices[MAX_INPUT_DEVICES];
    size_t device_count;
} GuiApp;

typedef struct StateUpdate
{
    GuiApp *app;
    bool running;
    char reason[128];
} StateUpdate;

static GtkWidget *row(GtkWidget *grid, int y, const char *label_text, GtkWidget *value)
{
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    if (GTK_IS_LABEL(value))
    {
        gtk_label_set_xalign(GTK_LABEL(value), 0.0f);
        gtk_widget_set_halign(value, GTK_ALIGN_START);
    }
    else if (GTK_IS_SWITCH(value))
    {
        gtk_widget_set_halign(value, GTK_ALIGN_START);
    }
    else
    {
        gtk_widget_set_hexpand(value, TRUE);
        gtk_widget_set_halign(value, GTK_ALIGN_FILL);
    }

    gtk_grid_attach(GTK_GRID(grid), label, 0, y, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), value, 1, y, 1, 1);
    return value;
}

static GtkWidget *section_label(const char *text)
{
    GtkWidget *label = gtk_label_new(NULL);
    char markup[128];

    snprintf(markup, sizeof(markup), "<b>%s</b>", text);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label, 14);
    return label;
}

static void config_from_widgets(GuiApp *app)
{
    const char *device_id;

    app->config.calibration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->calibration_spin));
    app->config.steps = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->steps_spin));
    app->config.step_delay_us = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->step_delay_spin));
    app->config.click_enabled = gtk_switch_get_active(GTK_SWITCH(app->click_switch));
    app->config.click_delay_ms = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->click_delay_spin));
    app->config.performance_max = true;

    device_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(app->device_combo));
    snprintf(app->config.keyboard_device,
             sizeof(app->config.keyboard_device),
             "%s",
             device_id ? device_id : "auto");
}

static void widgets_from_config(GuiApp *app)
{
    char key_name[64];
    char key_text[96];

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->calibration_spin), app->config.calibration);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->steps_spin), app->config.steps);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->step_delay_spin), app->config.step_delay_us);
    gtk_switch_set_active(GTK_SWITCH(app->click_switch), app->config.click_enabled);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->click_delay_spin), app->config.click_delay_ms);

    snprintf(key_text,
             sizeof(key_text),
             "%s (%d)",
             input_key_name(app->config.key_code, key_name, sizeof(key_name)),
             app->config.key_code);
    gtk_label_set_text(GTK_LABEL(app->key_label), key_text);
}

static void update_status(GuiApp *app, bool running, const char *reason)
{
    gtk_label_set_text(GTK_LABEL(app->status_label), running ? "RUNNING" : "STOPPED");
    gtk_label_set_text(GTK_LABEL(app->macro_diag_label), running ? "RUNNING" : "STOPPED");
    gtk_widget_set_sensitive(app->start_button, !running);
    gtk_widget_set_sensitive(app->stop_button, running);
    if (reason && *reason)
        gtk_widget_set_tooltip_text(app->status_label, reason);
}

static void update_diagnostics(GuiApp *app)
{
    Display *display = NULL;
    int major = 0;
    int minor = 0;
    char error[256] = "";
    char detected_path[PATH_MAX] = "";
    char detected_name[256] = "";
    bool x11_ok;
    bool xtest_ok = false;
    bool evdev_ok;
    bool uinput_ok;

    x11_ok = camera_open_display(&display, error, sizeof(error));
    if (x11_ok)
    {
        xtest_ok = camera_xtest_available(display, &major, &minor, error, sizeof(error));
        XCloseDisplay(display);
    }
    evdev_ok = input_evdev_available_for_key(app->config.key_code);
    uinput_ok = performance_uinput_available();

    gtk_label_set_text(GTK_LABEL(app->x11_label), x11_ok ? "OK" : "FAILED");
    if (xtest_ok)
    {
        char text[64];
        snprintf(text, sizeof(text), "OK (%d.%d)", major, minor);
        gtk_label_set_text(GTK_LABEL(app->xtest_label), text);
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(app->xtest_label), "FAILED");
    }
    gtk_label_set_text(GTK_LABEL(app->evdev_label), evdev_ok ? "OK" : "FAILED");
    gtk_label_set_text(GTK_LABEL(app->uinput_label), uinput_ok ? "AVAILABLE" : "FAILED");

    if (input_auto_detect_device_for_key(app->config.key_code, detected_path, sizeof(detected_path), detected_name, sizeof(detected_name)))
    {
        char text[PATH_MAX + 300];
        snprintf(text, sizeof(text), "%s\n%s", detected_name, detected_path);
        gtk_label_set_text(GTK_LABEL(app->detected_label), text);
        gtk_label_set_text(GTK_LABEL(app->keyboard_diag_label), text);
    }
    else
    {
        char key_name[64];
        char text[160];

        snprintf(text,
                 sizeof(text),
                 "No readable %s keyboard found",
                 input_key_name(app->config.key_code, key_name, sizeof(key_name)));
        gtk_label_set_text(GTK_LABEL(app->detected_label), text);
        gtk_label_set_text(GTK_LABEL(app->keyboard_diag_label), text);
    }
}

static void refresh_devices(GuiApp *app)
{
    GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(app->device_combo);
    char active[PATH_MAX];
    const char *configured = app->config.keyboard_device;

    if (!configured[0])
        configured = "auto";
    snprintf(active, sizeof(active), "%s", configured);

    gtk_combo_box_text_remove_all(combo);
    gtk_combo_box_text_append(combo, "auto", "Auto Detect");

    app->device_count = input_scan_devices_for_key(app->config.key_code, app->devices, MAX_INPUT_DEVICES);
    for (size_t i = 0; i < app->device_count; ++i)
    {
        char text[PATH_MAX + 300];
        snprintf(text, sizeof(text), "%s (%s)", app->devices[i].name, app->devices[i].path);
        gtk_combo_box_text_append(combo, app->devices[i].path, text);
    }

    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), active))
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), "auto");

    update_diagnostics(app);
}

static gboolean state_update_on_main(gpointer user_data)
{
    StateUpdate *update = user_data;

    update_status(update->app, update->running, update->reason);
    update_diagnostics(update->app);
    g_free(update);
    return G_SOURCE_REMOVE;
}

static void macro_state_changed(bool running, const char *reason, void *user_data)
{
    StateUpdate *update = g_new0(StateUpdate, 1);

    update->app = user_data;
    update->running = running;
    snprintf(update->reason, sizeof(update->reason), "%s", reason ? reason : "");
    g_main_context_invoke(NULL, state_update_on_main, update);
}

static void show_error(GtkWindow *parent, const char *message)
{
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s",
                                               message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void on_start_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;
    MacroCallbacks callbacks = {
        .state_changed = macro_state_changed,
        .user_data = app,
    };
    char error[512] = "";

    (void)button;
    config_from_widgets(app);
    if (!input_start_macro(&app->config, &callbacks, error, sizeof(error)))
        show_error(GTK_WINDOW(app->window), error);
}

static void on_stop_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;

    (void)button;
    input_stop_macro();
    update_status(app, false, "Stopped.");
    update_diagnostics(app);
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;

    (void)button;
    config_from_widgets(app);
    refresh_devices(app);
}

static void on_record_key_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;
    KeyRecord record;
    char error[512] = "";
    const char *device_id;

    (void)button;
    if (input_is_running())
    {
        show_error(GTK_WINDOW(app->window), "Stop the macro before recording a keybind.");
        return;
    }

    config_from_widgets(app);
    device_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(app->device_combo));
    gtk_button_set_label(GTK_BUTTON(app->record_key_button), "Press a key...");
    gtk_widget_set_sensitive(app->record_key_button, FALSE);
    while (gtk_events_pending())
        gtk_main_iteration();

    if (!input_record_next_key(device_id ? device_id : "auto", 10000, &record, error, sizeof(error)))
    {
        gtk_button_set_label(GTK_BUTTON(app->record_key_button), "Record Key");
        gtk_widget_set_sensitive(app->record_key_button, TRUE);
        show_error(GTK_WINDOW(app->window), error);
        return;
    }

    app->config.key_code = record.key_code;
    snprintf(app->config.keyboard_device, sizeof(app->config.keyboard_device), "%s", record.path);
    widgets_from_config(app);
    refresh_devices(app);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(app->device_combo), record.path);
    gtk_button_set_label(GTK_BUTTON(app->record_key_button), "Record Key");
    gtk_widget_set_sensitive(app->record_key_button, TRUE);
}

static void on_save_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;
    char error[512] = "";

    (void)button;
    config_from_widgets(app);
    if (!config_save(&app->config, error, sizeof(error)))
        show_error(GTK_WINDOW(app->window), error);
}

static void on_reset_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;

    (void)button;
    config_set_defaults(&app->config);
    widgets_from_config(app);
    refresh_devices(app);
}

static void on_test_flip_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;
    char error[512] = "";

    (void)button;
    config_from_widgets(app);
    if (!input_test_flip(&app->config, error, sizeof(error)))
        show_error(GTK_WINDOW(app->window), error);
}

static void on_test_click_clicked(GtkButton *button, gpointer user_data)
{
    GuiApp *app = user_data;
    char error[512] = "";

    (void)button;
    if (!input_test_click(error, sizeof(error)))
        show_error(GTK_WINDOW(app->window), error);
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget;
    (void)event;
    (void)user_data;
    input_stop_macro();
    return FALSE;
}

static GtkWidget *make_spin(double min, double max, double step)
{
    GtkWidget *spin = gtk_spin_button_new_with_range(min, max, step);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_entry_set_width_chars(GTK_ENTRY(spin), 10);
    return spin;
}

static GtkWidget *make_panel_grid(void)
{
    GtkWidget *grid = gtk_grid_new();

    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, FALSE);

    return grid;
}

int gui_run(int argc, char **argv)
{
    GuiApp app;
    GtkWidget *outer;
    GtkWidget *header;
    GtkWidget *columns;
    GtkWidget *left_grid;
    GtkWidget *right_grid;
    GtkWidget *buttons;
    GtkWidget *label;
    GtkWidget *refresh_button;
    GtkWidget *test_flip_button;
    GtkWidget *test_click_button;
    GtkWidget *save_button;
    GtkWidget *reset_button;
    GtkWidget *settings_buttons;
    char error[512] = "";

    memset(&app, 0, sizeof(app));

    if (!config_load(&app.config, error, sizeof(error)))
        fprintf(stderr, "Config load warning: %s\n", error);

    if (!gtk_init_check(&argc, &argv))
    {
        fprintf(stderr, "Could not initialize GTK on X11.\n");
        return 1;
    }

    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app.window), 860, 460);
    gtk_window_set_resizable(GTK_WINDOW(app.window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 14);
    g_signal_connect(app.window, "delete-event", G_CALLBACK(on_window_delete), &app);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(app.window), outer);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(outer), header, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<span size=\"x-large\"><b>Curve Macro</b></span>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_pack_start(GTK_BOX(header), label, TRUE, TRUE, 0);

    app.status_label = gtk_label_new("STOPPED");
    gtk_widget_set_halign(app.status_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(app.status_label), 0.0f);

    label = gtk_label_new("Status:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(header), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), app.status_label, FALSE, FALSE, 0);

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app.start_button = gtk_button_new_with_label("Start Macro");
    app.stop_button = gtk_button_new_with_label("Stop Macro");
    gtk_box_pack_start(GTK_BOX(buttons), app.start_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), app.stop_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), buttons, FALSE, FALSE, 0);

    columns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_box_pack_start(GTK_BOX(outer), columns, TRUE, TRUE, 0);

    left_grid = make_panel_grid();
    right_grid = make_panel_grid();
    gtk_box_pack_start(GTK_BOX(columns), left_grid, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(columns), right_grid, TRUE, TRUE, 0);

    gtk_grid_attach(GTK_GRID(left_grid), section_label("Camera"), 0, 0, 2, 1);
    app.calibration_spin = make_spin(1, 100000, 1);
    app.steps_spin = make_spin(1, 1024, 1);
    app.step_delay_spin = make_spin(0, 100000, 1);
    row(left_grid, 1, "180-deg Calibration:", app.calibration_spin);
    row(left_grid, 2, "Steps:", app.steps_spin);
    row(left_grid, 3, "Step Delay (microseconds):", app.step_delay_spin);

    gtk_grid_attach(GTK_GRID(left_grid), section_label("Automatic Click"), 0, 4, 2, 1);
    app.click_switch = gtk_switch_new();
    app.click_delay_spin = make_spin(0, 10000, 1);
    row(left_grid, 5, "Enable Click:", app.click_switch);
    row(left_grid, 6, "Click Delay (milliseconds):", app.click_delay_spin);

    gtk_grid_attach(GTK_GRID(left_grid), section_label("Keyboard"), 0, 7, 2, 1);
    app.device_combo = gtk_combo_box_text_new();
    app.detected_label = gtk_label_new("");
    gtk_widget_set_halign(app.detected_label, GTK_ALIGN_START);
    row(left_grid, 8, "Keyboard Device:", app.device_combo);
    row(left_grid, 9, "Detected Device:", app.detected_label);
    app.key_label = gtk_label_new("");
    row(left_grid, 10, "Keybind:", app.key_label);

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    refresh_button = gtk_button_new_with_label("Refresh Devices");
    app.record_key_button = gtk_button_new_with_label("Record Key");
    gtk_box_pack_start(GTK_BOX(buttons), refresh_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), app.record_key_button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(left_grid), buttons, 0, 11, 2, 1);

    gtk_grid_attach(GTK_GRID(right_grid), section_label("Performance"), 0, 0, 2, 1);
    row(right_grid, 1, "Input Backend:", gtk_label_new("evdev + XTest"));
    app.x11_label = gtk_label_new("");
    app.xtest_label = gtk_label_new("");
    app.evdev_label = gtk_label_new("");
    app.uinput_label = gtk_label_new("");
    row(right_grid, 2, "X11:", app.x11_label);
    row(right_grid, 3, "XTest:", app.xtest_label);
    row(right_grid, 4, "evdev:", app.evdev_label);
    row(right_grid, 5, "uinput:", app.uinput_label);
    row(right_grid, 6, "Latency Mode:", gtk_label_new("Maximum Performance"));

    gtk_grid_attach(GTK_GRID(right_grid), section_label("Diagnostics"), 0, 7, 2, 1);
    app.keyboard_diag_label = gtk_label_new("");
    app.macro_diag_label = gtk_label_new("STOPPED");
    gtk_widget_set_halign(app.keyboard_diag_label, GTK_ALIGN_START);
    row(right_grid, 8, "Keyboard:", app.keyboard_diag_label);
    row(right_grid, 9, "Macro:", app.macro_diag_label);

    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    settings_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    test_flip_button = gtk_button_new_with_label("Test Flip");
    test_click_button = gtk_button_new_with_label("Test Click");
    save_button = gtk_button_new_with_label("Save Settings");
    reset_button = gtk_button_new_with_label("Reset Defaults");
    gtk_box_pack_start(GTK_BOX(buttons), test_flip_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), test_click_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(settings_buttons), save_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(settings_buttons), reset_button, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(right_grid), buttons, 0, 10, 2, 1);
    gtk_grid_attach(GTK_GRID(right_grid), settings_buttons, 0, 11, 2, 1);

    g_signal_connect(app.start_button, "clicked", G_CALLBACK(on_start_clicked), &app);
    g_signal_connect(app.stop_button, "clicked", G_CALLBACK(on_stop_clicked), &app);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), &app);
    g_signal_connect(app.record_key_button, "clicked", G_CALLBACK(on_record_key_clicked), &app);
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked), &app);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_clicked), &app);
    g_signal_connect(test_flip_button, "clicked", G_CALLBACK(on_test_flip_clicked), &app);
    g_signal_connect(test_click_button, "clicked", G_CALLBACK(on_test_click_clicked), &app);

    widgets_from_config(&app);
    refresh_devices(&app);
    update_status(&app, false, "Stopped.");

    gtk_widget_show_all(app.window);
    gtk_main();
    input_stop_macro();

    return 0;
}
