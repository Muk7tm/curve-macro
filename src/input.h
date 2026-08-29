#ifndef ROBLOX_CAMERA_FLIP_INPUT_H
#define ROBLOX_CAMERA_FLIP_INPUT_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>

#define MAX_INPUT_DEVICES 64

typedef struct InputDevice
{
    char path[PATH_MAX];
    char name[256];
    bool readable;
    int score;
} InputDevice;

typedef struct MacroCallbacks
{
    void (*state_changed)(bool running, const char *reason, void *user_data);
    void *user_data;
} MacroCallbacks;

size_t input_scan_devices(InputDevice *devices, size_t capacity);
bool input_auto_detect_device(char *path, size_t path_size, char *name, size_t name_size);
bool input_start_macro(const AppConfig *config,
                       const MacroCallbacks *callbacks,
                       char *error,
                       size_t error_size);
void input_stop_macro(void);
bool input_is_running(void);
bool input_test_flip(const AppConfig *config, char *error, size_t error_size);
bool input_test_click(char *error, size_t error_size);
bool input_evdev_available(void);

#endif
