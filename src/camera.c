#include "camera.h"

#include <X11/extensions/XTest.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

static void camera_set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static bool add_us_to_timespec(struct timespec *ts, long us)
{
    long nsec;

    if (!ts || us < 0)
        return false;

    ts->tv_sec += us / 1000000L;
    nsec = ts->tv_nsec + (us % 1000000L) * 1000L;
    if (nsec >= 1000000000L)
    {
        ts->tv_sec += nsec / 1000000000L;
        ts->tv_nsec = nsec % 1000000000L;
    }
    else
    {
        ts->tv_nsec = nsec;
    }

    return true;
}

static bool sleep_until_cancelable(const struct timespec *deadline,
                                   CameraCancelFunc cancel_func,
                                   void *cancel_user_data)
{
    int rc;

    while (true)
    {
        if (cancel_func && cancel_func(cancel_user_data))
            return false;

        rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
        if (rc == 0)
            return !(cancel_func && cancel_func(cancel_user_data));
        if (rc != EINTR)
            return false;
    }
}

static bool sleep_us_cancelable(long us, CameraCancelFunc cancel_func, void *cancel_user_data)
{
    struct timespec deadline;

    if (us <= 0)
        return !(cancel_func && cancel_func(cancel_user_data));

    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0)
        return false;

    if (!add_us_to_timespec(&deadline, us))
        return false;

    return sleep_until_cancelable(&deadline, cancel_func, cancel_user_data);
}

bool camera_sleep_ms(int ms, CameraCancelFunc cancel_func, void *cancel_user_data)
{
    struct timespec deadline;

    if (ms <= 0)
        return !(cancel_func && cancel_func(cancel_user_data));

    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0)
        return false;

    if (!add_us_to_timespec(&deadline, (long)ms * 1000L))
        return false;

    while (true)
    {
        struct timespec now;
        struct timespec slice_deadline;
        long remaining_us;

        if (cancel_func && cancel_func(cancel_user_data))
            return false;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
            return false;

        remaining_us = (deadline.tv_sec - now.tv_sec) * 1000000L +
                       (deadline.tv_nsec - now.tv_nsec) / 1000L;
        if (remaining_us <= 0)
            return true;

        slice_deadline = now;
        if (!add_us_to_timespec(&slice_deadline, remaining_us > 1000L ? 1000L : remaining_us))
            return false;

        if (!sleep_until_cancelable(&slice_deadline, cancel_func, cancel_user_data))
            return false;
    }
}

bool camera_open_display(Display **display, char *error, size_t error_size)
{
    if (!display)
    {
        camera_set_error(error, error_size, "Invalid X11 display pointer.");
        return false;
    }

    *display = XOpenDisplay(NULL);
    if (!*display)
    {
        camera_set_error(error, error_size, "Could not open X11 display. Run this under an X11 session.");
        return false;
    }

    return true;
}

bool camera_xtest_available(Display *display, int *major, int *minor, char *error, size_t error_size)
{
    int event_base;
    int error_base;
    int local_major = 2;
    int local_minor = 2;

    if (!display)
    {
        camera_set_error(error, error_size, "X11 display is not open.");
        return false;
    }

    if (!XTestQueryExtension(display, &event_base, &error_base, &local_major, &local_minor))
    {
        camera_set_error(error, error_size, "XTest extension is not available.");
        return false;
    }

    if (major)
        *major = local_major;
    if (minor)
        *minor = local_minor;

    return true;
}

bool camera_perform_turn(Display *display,
                         int total,
                         int steps,
                         int step_delay_us,
                         int direction,
                         CameraCancelFunc cancel_func,
                         void *cancel_user_data,
                         char *error,
                         size_t error_size)
{
    int base;
    int remainder;

    if (!display)
    {
        camera_set_error(error, error_size, "X11 display is not open.");
        return false;
    }
    if (total <= 0 || steps <= 0 || step_delay_us < 0)
    {
        camera_set_error(error, error_size, "Invalid camera timing settings.");
        return false;
    }
    if (direction != 1 && direction != -1)
    {
        camera_set_error(error, error_size, "Invalid turn direction.");
        return false;
    }

    base = total / steps;
    remainder = total % steps;

    for (int i = 0; i < steps; ++i)
    {
        int amount;

        if (cancel_func && cancel_func(cancel_user_data))
        {
            camera_set_error(error, error_size, "Camera turn canceled.");
            return false;
        }

        amount = base + (i < remainder ? 1 : 0);
        amount *= direction;

        /*
         * Preserve the known-good mechanism: relative horizontal XTest motion.
         * dy is always zero, and each step is flushed to avoid visible buffering.
         */
        if (!XTestFakeRelativeMotionEvent(display, amount, 0, CurrentTime))
        {
            camera_set_error(error, error_size, "XTest relative motion failed.");
            return false;
        }
        XFlush(display);

        if (step_delay_us > 0 && !sleep_us_cancelable(step_delay_us, cancel_func, cancel_user_data))
        {
            camera_set_error(error, error_size, "Camera turn canceled.");
            return false;
        }
    }

    XFlush(display);
    return true;
}

bool camera_left_click(Display *display,
                       CameraCancelFunc cancel_func,
                       void *cancel_user_data,
                       char *error,
                       size_t error_size)
{
    if (!display)
    {
        camera_set_error(error, error_size, "X11 display is not open.");
        return false;
    }
    if (cancel_func && cancel_func(cancel_user_data))
    {
        camera_set_error(error, error_size, "Click canceled.");
        return false;
    }

    if (!XTestFakeButtonEvent(display, 1, True, CurrentTime))
    {
        camera_set_error(error, error_size, "Left button press failed.");
        return false;
    }
    XFlush(display);

    if (!sleep_us_cancelable(1000L, cancel_func, cancel_user_data))
    {
        XTestFakeButtonEvent(display, 1, False, CurrentTime);
        XFlush(display);
        camera_set_error(error, error_size, "Click canceled.");
        return false;
    }

    if (!XTestFakeButtonEvent(display, 1, False, CurrentTime))
    {
        camera_set_error(error, error_size, "Left button release failed.");
        return false;
    }
    XFlush(display);

    return true;
}

void camera_release_buttons(Display *display)
{
    if (!display)
        return;

    XTestFakeButtonEvent(display, 1, False, CurrentTime);
    XTestFakeButtonEvent(display, 3, False, CurrentTime);
    XFlush(display);
}
