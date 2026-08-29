#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

static void config_set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

void config_set_defaults(AppConfig *config)
{
    if (!config)
        return;

    config->calibration = DEFAULT_CALIBRATION;
    config->steps = DEFAULT_STEPS;
    config->step_delay_us = DEFAULT_STEP_DELAY_US;
    config->click_enabled = DEFAULT_CLICK_ENABLED;
    config->click_delay_ms = DEFAULT_CLICK_DELAY_MS;
    snprintf(config->keyboard_device, sizeof(config->keyboard_device), "%s", "auto");
    config->key_code = DEFAULT_KEY_CODE;
    config->performance_max = DEFAULT_PERFORMANCE_MAX;
}

bool config_get_path(char *path, size_t path_size, char *error, size_t error_size)
{
    const char *config_home = g_get_user_config_dir();
    int written;

    if (!path || path_size == 0)
    {
        config_set_error(error, error_size, "Invalid config path buffer.");
        return false;
    }

    written = snprintf(path, path_size, "%s/%s/config.ini", config_home, APP_ID);
    if (written < 0 || (size_t)written >= path_size)
    {
        config_set_error(error, error_size, "Config path is too long.");
        return false;
    }

    return true;
}

static bool ensure_config_dir(char *error, size_t error_size)
{
    const char *config_home = g_get_user_config_dir();
    char dir[PATH_MAX];
    int written = snprintf(dir, sizeof(dir), "%s/%s", config_home, APP_ID);

    if (written < 0 || (size_t)written >= sizeof(dir))
    {
        config_set_error(error, error_size, "Config directory path is too long.");
        return false;
    }

    if (g_mkdir_with_parents(dir, 0700) != 0)
    {
        config_set_error(error, error_size, "Could not create config directory.");
        return false;
    }

    return true;
}

bool config_load(AppConfig *config, char *error, size_t error_size)
{
    GKeyFile *key_file;
    GError *g_error = NULL;
    char path[PATH_MAX];
    char *device;

    config_set_defaults(config);

    if (!config_get_path(path, sizeof(path), error, error_size))
        return false;

    if (!g_file_test(path, G_FILE_TEST_EXISTS))
        return true;

    key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &g_error))
    {
        config_set_error(error, error_size, g_error ? g_error->message : "Could not load config.");
        g_clear_error(&g_error);
        g_key_file_unref(key_file);
        return false;
    }

    if (g_key_file_has_key(key_file, "camera", "calibration", NULL))
        config->calibration = g_key_file_get_integer(key_file, "camera", "calibration", NULL);
    if (g_key_file_has_key(key_file, "camera", "steps", NULL))
        config->steps = g_key_file_get_integer(key_file, "camera", "steps", NULL);
    if (g_key_file_has_key(key_file, "camera", "step_delay_us", NULL))
        config->step_delay_us = g_key_file_get_integer(key_file, "camera", "step_delay_us", NULL);

    if (g_key_file_has_key(key_file, "click", "enabled", NULL))
        config->click_enabled = g_key_file_get_boolean(key_file, "click", "enabled", NULL);
    if (g_key_file_has_key(key_file, "click", "delay_ms", NULL))
        config->click_delay_ms = g_key_file_get_integer(key_file, "click", "delay_ms", NULL);

    device = g_key_file_get_string(key_file, "keyboard", "device", NULL);
    if (device)
    {
        snprintf(config->keyboard_device, sizeof(config->keyboard_device), "%s", device);
        g_free(device);
    }
    if (g_key_file_has_key(key_file, "keyboard", "key", NULL))
        config->key_code = g_key_file_get_integer(key_file, "keyboard", "key", NULL);

    if (g_key_file_has_key(key_file, "performance", "mode", NULL))
    {
        char *mode = g_key_file_get_string(key_file, "performance", "mode", NULL);
        config->performance_max = !mode || strcmp(mode, "maximum") == 0;
        g_free(mode);
    }

    if (config->calibration <= 0)
        config->calibration = DEFAULT_CALIBRATION;
    if (config->steps <= 0)
        config->steps = DEFAULT_STEPS;
    if (config->step_delay_us < 0)
        config->step_delay_us = DEFAULT_STEP_DELAY_US;
    if (config->click_delay_ms < 0)
        config->click_delay_ms = DEFAULT_CLICK_DELAY_MS;
    if (config->key_code <= 0 || config->key_code > KEY_MAX)
        config->key_code = DEFAULT_KEY_CODE;

    g_key_file_unref(key_file);
    return true;
}

bool config_save(const AppConfig *config, char *error, size_t error_size)
{
    GKeyFile *key_file;
    GError *g_error = NULL;
    char path[PATH_MAX];
    gchar *data;
    gsize length;
    bool ok;

    if (!ensure_config_dir(error, error_size))
        return false;
    if (!config_get_path(path, sizeof(path), error, error_size))
        return false;

    key_file = g_key_file_new();
    g_key_file_set_integer(key_file, "camera", "calibration", config->calibration);
    g_key_file_set_integer(key_file, "camera", "steps", config->steps);
    g_key_file_set_integer(key_file, "camera", "step_delay_us", config->step_delay_us);
    g_key_file_set_boolean(key_file, "click", "enabled", config->click_enabled);
    g_key_file_set_integer(key_file, "click", "delay_ms", config->click_delay_ms);
    g_key_file_set_string(key_file, "keyboard", "device", config->keyboard_device);
    g_key_file_set_integer(key_file, "keyboard", "key", config->key_code);
    g_key_file_set_string(key_file, "performance", "mode", config->performance_max ? "maximum" : "normal");

    data = g_key_file_to_data(key_file, &length, NULL);
    ok = g_file_set_contents(path, data, (gssize)length, &g_error);
    if (!ok)
    {
        config_set_error(error, error_size, g_error ? g_error->message : "Could not save config.");
        g_clear_error(&g_error);
    }

    g_free(data);
    g_key_file_unref(key_file);
    return ok;
}
