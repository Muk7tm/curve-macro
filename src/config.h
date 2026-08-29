#ifndef ROBLOX_CAMERA_FLIP_CONFIG_H
#define ROBLOX_CAMERA_FLIP_CONFIG_H

#include <linux/input-event-codes.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define APP_ID "roblox-camera-flip"
#define APP_TITLE "Roblox Camera Flip"

#define DEFAULT_CALIBRATION 2500
#define DEFAULT_STEPS 32
#define DEFAULT_STEP_DELAY_US 500
#define DEFAULT_CLICK_ENABLED true
#define DEFAULT_CLICK_DELAY_MS 0
#define DEFAULT_KEY_CODE KEY_C
#define DEFAULT_PERFORMANCE_MAX true

typedef struct AppConfig
{
    int calibration;
    int steps;
    int step_delay_us;
    bool click_enabled;
    int click_delay_ms;
    char keyboard_device[PATH_MAX];
    int key_code;
    bool performance_max;
} AppConfig;

void config_set_defaults(AppConfig *config);
bool config_load(AppConfig *config, char *error, size_t error_size);
bool config_save(const AppConfig *config, char *error, size_t error_size);
bool config_get_path(char *path, size_t path_size, char *error, size_t error_size);

#endif
