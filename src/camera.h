#ifndef CURVE_MACRO_CAMERA_H
#define CURVE_MACRO_CAMERA_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stddef.h>

typedef bool (*CameraCancelFunc)(void *user_data);

bool camera_open_display(Display **display, char *error, size_t error_size);
bool camera_xtest_available(Display *display, int *major, int *minor, char *error, size_t error_size);
bool camera_perform_turn(Display *display,
                         int total,
                         int steps,
                         int step_delay_us,
                         int direction,
                         CameraCancelFunc cancel_func,
                         void *cancel_user_data,
                         char *error,
                         size_t error_size);
bool camera_left_click(Display *display,
                       CameraCancelFunc cancel_func,
                       void *cancel_user_data,
                       char *error,
                       size_t error_size);
void camera_release_buttons(Display *display);
bool camera_sleep_ms(int ms, CameraCancelFunc cancel_func, void *cancel_user_data);

#endif
