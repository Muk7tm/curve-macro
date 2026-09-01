#ifndef CURVE_MACRO_INPUT_H
#define CURVE_MACRO_INPUT_H

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

typedef struct KeyRecord
{
    char path[PATH_MAX];
    char device_name[256];
    int key_code;
    char key_name[64];
} KeyRecord;

typedef struct MacroCallbacks
{
    void (*state_changed)(bool running, const char *reason, void *user_data);
    void *user_data;
} MacroCallbacks;

size_t input_scan_devices(InputDevice *devices, size_t capacity);
size_t input_scan_devices_for_key(int key_code, InputDevice *devices, size_t capacity);
bool input_auto_detect_device(char *path, size_t path_size, char *name, size_t name_size);
bool input_auto_detect_device_for_key(int key_code, char *path, size_t path_size, char *name, size_t name_size);
bool input_record_next_key(const char *preferred_device,
                           int timeout_ms,
                           KeyRecord *record,
                           char *error,
                           size_t error_size);
const char *input_key_name(int key_code, char *buffer, size_t buffer_size);
bool input_start_macro(const AppConfig *config,
                       const MacroCallbacks *callbacks,
                       char *error,
                       size_t error_size);
void input_stop_macro(void);
bool input_is_running(void);
bool input_test_flip(const AppConfig *config, char *error, size_t error_size);
bool input_test_click(char *error, size_t error_size);
bool input_evdev_available(void);
bool input_evdev_available_for_key(int key_code);

#endif
