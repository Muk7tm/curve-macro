#include "input.h"

#include "camera.h"
#include "performance.h"

#include <X11/Xlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8U)
#define NBITS(x) ((((x) - 1U) / BITS_PER_LONG) + 1U)

typedef struct MacroRuntime
{
    pthread_mutex_t mutex;
    pthread_t thread;
    bool thread_valid;
    bool running;
    atomic_bool stop_requested;
    int stop_pipe[2];
    int keyboard_fd;
    Display *display;
    AppConfig config;
    MacroCallbacks callbacks;
} MacroRuntime;

typedef struct PollCancelContext
{
    MacroRuntime *runtime;
    bool c_released;
    bool emergency;
} PollCancelContext;

static MacroRuntime runtime = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .thread_valid = false,
    .running = false,
    .stop_pipe = {-1, -1},
    .keyboard_fd = -1,
    .display = NULL,
};

static void input_set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size > 0)
        snprintf(error, error_size, "%s", message);
}

static bool test_bit(int bit, const unsigned long *array)
{
    return (array[bit / (int)BITS_PER_LONG] & (1UL << (bit % (int)BITS_PER_LONG))) != 0;
}

static bool device_has_ev_key(int fd)
{
    unsigned long ev_bits[NBITS(EV_MAX)] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0)
        return false;

    return test_bit(EV_KEY, ev_bits);
}

static bool device_supports_key(int fd, int key_code)
{
    unsigned long key_bits[NBITS(KEY_MAX)] = {0};

    if (!device_has_ev_key(fd))
        return false;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0)
        return false;

    return test_bit(key_code, key_bits);
}

static bool is_bindable_keyboard_key(int key_code)
{
    return key_code > KEY_RESERVED && key_code < BTN_MISC && key_code != KEY_F12;
}

const char *input_key_name(int key_code, char *buffer, size_t buffer_size)
{
    const char *name = NULL;

    switch (key_code)
    {
    case KEY_A: name = "KEY_A"; break;
    case KEY_B: name = "KEY_B"; break;
    case KEY_C: name = "KEY_C"; break;
    case KEY_D: name = "KEY_D"; break;
    case KEY_E: name = "KEY_E"; break;
    case KEY_F: name = "KEY_F"; break;
    case KEY_G: name = "KEY_G"; break;
    case KEY_H: name = "KEY_H"; break;
    case KEY_I: name = "KEY_I"; break;
    case KEY_J: name = "KEY_J"; break;
    case KEY_K: name = "KEY_K"; break;
    case KEY_L: name = "KEY_L"; break;
    case KEY_M: name = "KEY_M"; break;
    case KEY_N: name = "KEY_N"; break;
    case KEY_O: name = "KEY_O"; break;
    case KEY_P: name = "KEY_P"; break;
    case KEY_Q: name = "KEY_Q"; break;
    case KEY_R: name = "KEY_R"; break;
    case KEY_S: name = "KEY_S"; break;
    case KEY_T: name = "KEY_T"; break;
    case KEY_U: name = "KEY_U"; break;
    case KEY_V: name = "KEY_V"; break;
    case KEY_W: name = "KEY_W"; break;
    case KEY_X: name = "KEY_X"; break;
    case KEY_Y: name = "KEY_Y"; break;
    case KEY_Z: name = "KEY_Z"; break;
    case KEY_1: name = "KEY_1"; break;
    case KEY_2: name = "KEY_2"; break;
    case KEY_3: name = "KEY_3"; break;
    case KEY_4: name = "KEY_4"; break;
    case KEY_5: name = "KEY_5"; break;
    case KEY_6: name = "KEY_6"; break;
    case KEY_7: name = "KEY_7"; break;
    case KEY_8: name = "KEY_8"; break;
    case KEY_9: name = "KEY_9"; break;
    case KEY_0: name = "KEY_0"; break;
    case KEY_SPACE: name = "KEY_SPACE"; break;
    case KEY_LEFTSHIFT: name = "KEY_LEFTSHIFT"; break;
    case KEY_RIGHTSHIFT: name = "KEY_RIGHTSHIFT"; break;
    case KEY_LEFTCTRL: name = "KEY_LEFTCTRL"; break;
    case KEY_RIGHTCTRL: name = "KEY_RIGHTCTRL"; break;
    case KEY_LEFTALT: name = "KEY_LEFTALT"; break;
    case KEY_RIGHTALT: name = "KEY_RIGHTALT"; break;
    case KEY_TAB: name = "KEY_TAB"; break;
    case KEY_CAPSLOCK: name = "KEY_CAPSLOCK"; break;
    case KEY_GRAVE: name = "KEY_GRAVE"; break;
    case KEY_MINUS: name = "KEY_MINUS"; break;
    case KEY_EQUAL: name = "KEY_EQUAL"; break;
    case KEY_LEFTBRACE: name = "KEY_LEFTBRACE"; break;
    case KEY_RIGHTBRACE: name = "KEY_RIGHTBRACE"; break;
    case KEY_SEMICOLON: name = "KEY_SEMICOLON"; break;
    case KEY_APOSTROPHE: name = "KEY_APOSTROPHE"; break;
    case KEY_BACKSLASH: name = "KEY_BACKSLASH"; break;
    case KEY_COMMA: name = "KEY_COMMA"; break;
    case KEY_DOT: name = "KEY_DOT"; break;
    case KEY_SLASH: name = "KEY_SLASH"; break;
    case KEY_ESC: name = "KEY_ESC"; break;
    case KEY_ENTER: name = "KEY_ENTER"; break;
    case KEY_BACKSPACE: name = "KEY_BACKSPACE"; break;
    case KEY_F1: name = "KEY_F1"; break;
    case KEY_F2: name = "KEY_F2"; break;
    case KEY_F3: name = "KEY_F3"; break;
    case KEY_F4: name = "KEY_F4"; break;
    case KEY_F5: name = "KEY_F5"; break;
    case KEY_F6: name = "KEY_F6"; break;
    case KEY_F7: name = "KEY_F7"; break;
    case KEY_F8: name = "KEY_F8"; break;
    case KEY_F9: name = "KEY_F9"; break;
    case KEY_F10: name = "KEY_F10"; break;
    case KEY_F11: name = "KEY_F11"; break;
    case KEY_F12: name = "KEY_F12"; break;
    default:
        break;
    }

    if (name)
        return name;

    if (buffer && buffer_size > 0)
        snprintf(buffer, buffer_size, "KEY_%d", key_code);

    return buffer ? buffer : "KEY_UNKNOWN";
}

static int keyboard_score(int fd, const char *name, int target_key)
{
    int score = 0;

    if (device_supports_key(fd, target_key))
        score += 100;
    if (device_supports_key(fd, KEY_C))
        score += 10;
    if (device_supports_key(fd, KEY_A))
        score += 10;
    if (device_supports_key(fd, KEY_Z))
        score += 10;
    if (device_supports_key(fd, KEY_SPACE))
        score += 10;
    if (device_supports_key(fd, KEY_ENTER))
        score += 10;
    if (device_supports_key(fd, KEY_F12))
        score += 5;

    if (name)
    {
        char lower[256];
        size_t len = strlen(name);

        if (len >= sizeof(lower))
            len = sizeof(lower) - 1;
        for (size_t i = 0; i < len; ++i)
        {
            char ch = name[i];
            lower[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
        }
        lower[len] = '\0';

        if (strstr(lower, "keyboard") || strstr(lower, "kbd") || strstr(lower, " kb"))
            score += 30;
        if (strstr(lower, "gaming") || strstr(lower, "tech"))
            score += 10;
    }

    return score;
}

static int compare_devices(const void *a, const void *b)
{
    const InputDevice *da = a;
    const InputDevice *db = b;

    if (da->score != db->score)
        return db->score - da->score;
    return strcmp(da->path, db->path);
}

size_t input_scan_devices(InputDevice *devices, size_t capacity)
{
    return input_scan_devices_for_key(KEY_C, devices, capacity);
}

size_t input_scan_devices_for_key(int key_code, InputDevice *devices, size_t capacity)
{
    DIR *dir;
    struct dirent *entry;
    size_t count = 0;

    if (!devices || capacity == 0 || key_code <= 0 || key_code > KEY_MAX)
        return 0;

    dir = opendir("/dev/input");
    if (!dir)
        return 0;

    while ((entry = readdir(dir)) != NULL)
    {
        char path[PATH_MAX];
        char name[256] = "Unknown keyboard";
        int written;
        int fd;
        int score;

        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        written = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path))
            continue;

        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;

        if (!device_supports_key(fd, key_code))
        {
            close(fd);
            continue;
        }

        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
            snprintf(name, sizeof(name), "%s", "Unknown keyboard");

        score = keyboard_score(fd, name, key_code);
        close(fd);

        if (count < capacity)
        {
            snprintf(devices[count].path, sizeof(devices[count].path), "%s", path);
            snprintf(devices[count].name, sizeof(devices[count].name), "%s", name);
            devices[count].readable = true;
            devices[count].score = score;
            ++count;
        }
    }

    closedir(dir);
    qsort(devices, count, sizeof(devices[0]), compare_devices);
    return count;
}

bool input_auto_detect_device(char *path, size_t path_size, char *name, size_t name_size)
{
    return input_auto_detect_device_for_key(KEY_C, path, path_size, name, name_size);
}

bool input_auto_detect_device_for_key(int key_code, char *path, size_t path_size, char *name, size_t name_size)
{
    InputDevice devices[MAX_INPUT_DEVICES];
    size_t count = input_scan_devices_for_key(key_code, devices, MAX_INPUT_DEVICES);

    if (count == 0)
        return false;

    if (path && path_size > 0)
        snprintf(path, path_size, "%s", devices[0].path);
    if (name && name_size > 0)
        snprintf(name, name_size, "%s", devices[0].name);

    return true;
}

bool input_evdev_available(void)
{
    InputDevice devices[MAX_INPUT_DEVICES];
    return input_scan_devices(devices, MAX_INPUT_DEVICES) > 0;
}

bool input_evdev_available_for_key(int key_code)
{
    InputDevice devices[MAX_INPUT_DEVICES];
    return input_scan_devices_for_key(key_code, devices, MAX_INPUT_DEVICES) > 0;
}

static int open_record_devices(struct pollfd *fds, InputDevice *devices, size_t capacity)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir("/dev/input");
    if (!dir)
        return 0;

    while ((entry = readdir(dir)) != NULL && count < (int)capacity)
    {
        char path[PATH_MAX];
        char name[256] = "Unknown keyboard";
        int written;
        int fd;
        int score;

        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        written = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path))
            continue;

        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        if (!device_has_ev_key(fd))
        {
            close(fd);
            continue;
        }

        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
            snprintf(name, sizeof(name), "%s", "Unknown keyboard");

        score = keyboard_score(fd, name, KEY_C);
        if (score < 20)
        {
            close(fd);
            continue;
        }

        snprintf(devices[count].path, sizeof(devices[count].path), "%s", path);
        snprintf(devices[count].name, sizeof(devices[count].name), "%s", name);
        devices[count].readable = true;
        devices[count].score = score;
        fds[count].fd = fd;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        ++count;
    }

    closedir(dir);
    return count;
}

static void close_record_devices(struct pollfd *fds, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (fds[i].fd >= 0)
            close(fds[i].fd);
    }
}

static void drain_record_devices(struct pollfd *fds, int count)
{
    for (int i = 0; i < count; ++i)
    {
        struct input_event ev;

        while (read(fds[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev))
        {
        }
    }
}

bool input_record_next_key(const char *preferred_device,
                           int timeout_ms,
                           KeyRecord *record,
                           char *error,
                           size_t error_size)
{
    struct pollfd fds[MAX_INPUT_DEVICES];
    InputDevice devices[MAX_INPUT_DEVICES];
    int count;
    int rc;

    (void)preferred_device;

    if (!record)
    {
        input_set_error(error, error_size, "Missing key record output.");
        return false;
    }

    count = open_record_devices(fds, devices, MAX_INPUT_DEVICES);
    if (count <= 0)
    {
        input_set_error(error, error_size, "No readable keyboard event devices found.");
        return false;
    }

    drain_record_devices(fds, count);
    rc = poll(fds, (nfds_t)count, timeout_ms);
    if (rc <= 0)
    {
        close_record_devices(fds, count);
        input_set_error(error, error_size, rc == 0 ? "Timed out waiting for a key press." : "Could not poll keyboard devices.");
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        if (!(fds[i].revents & POLLIN))
            continue;

        while (true)
        {
            struct input_event ev;
            char key_name[64];
            ssize_t n = read(fds[i].fd, &ev, sizeof(ev));

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                close_record_devices(fds, count);
                input_set_error(error, error_size, "Could not read keyboard event while recording.");
                return false;
            }
            if (n != (ssize_t)sizeof(ev) || ev.type != EV_KEY || ev.value != 1)
                continue;
            if (ev.code == KEY_F12)
            {
                close_record_devices(fds, count);
                input_set_error(error, error_size, "F12 is reserved for emergency stop.");
                return false;
            }
            if (!is_bindable_keyboard_key(ev.code))
                continue;

            snprintf(record->path, sizeof(record->path), "%s", devices[i].path);
            snprintf(record->device_name, sizeof(record->device_name), "%s", devices[i].name);
            record->key_code = ev.code;
            snprintf(record->key_name, sizeof(record->key_name), "%s", input_key_name(ev.code, key_name, sizeof(key_name)));
            close_record_devices(fds, count);
            return true;
        }
    }

    close_record_devices(fds, count);
    input_set_error(error, error_size, "No bindable keyboard key was detected.");
    return false;
}

static void invoke_state_callback(bool running_state, const char *reason)
{
    MacroCallbacks callbacks;

    pthread_mutex_lock(&runtime.mutex);
    callbacks = runtime.callbacks;
    pthread_mutex_unlock(&runtime.mutex);

    if (callbacks.state_changed)
        callbacks.state_changed(running_state, reason, callbacks.user_data);
}

static bool poll_cancel_func(void *user_data)
{
    PollCancelContext *ctx = user_data;
    struct pollfd fds[2];
    int rc;

    if (!ctx || !ctx->runtime)
        return false;
    if (atomic_load(&ctx->runtime->stop_requested))
        return true;

    fds[0].fd = ctx->runtime->stop_pipe[0];
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = ctx->runtime->keyboard_fd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    rc = poll(fds, 2, 0);
    if (rc <= 0)
        return false;

    if (fds[0].revents & POLLIN)
    {
        char buffer[16];
        while (read(ctx->runtime->stop_pipe[0], buffer, sizeof(buffer)) > 0)
        {
        }
        atomic_store(&ctx->runtime->stop_requested, true);
        return true;
    }

    if (fds[1].revents & POLLIN)
    {
        struct input_event ev;

        while (read(ctx->runtime->keyboard_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev))
        {
            if (ev.type != EV_KEY)
                continue;
            if (ev.code == KEY_F12 && ev.value == 1)
            {
                ctx->emergency = true;
                atomic_store(&ctx->runtime->stop_requested, true);
                return true;
            }
            if (ev.code == ctx->runtime->config.key_code && ev.value == 0)
                ctx->c_released = true;
        }
    }

    return atomic_load(&ctx->runtime->stop_requested);
}

static void close_runtime_resources(void)
{
    if (runtime.display)
    {
        camera_release_buttons(runtime.display);
        XCloseDisplay(runtime.display);
        runtime.display = NULL;
    }
    if (runtime.keyboard_fd >= 0)
    {
        close(runtime.keyboard_fd);
        runtime.keyboard_fd = -1;
    }
    if (runtime.stop_pipe[0] >= 0)
    {
        close(runtime.stop_pipe[0]);
        runtime.stop_pipe[0] = -1;
    }
    if (runtime.stop_pipe[1] >= 0)
    {
        close(runtime.stop_pipe[1]);
        runtime.stop_pipe[1] = -1;
    }
}

static void *input_thread_main(void *unused)
{
    bool c_held = false;
    bool emergency = false;
    const char *stop_reason = "Stopped.";

    (void)unused;
    performance_apply_to_input_thread(runtime.config.performance_max);

    while (!atomic_load(&runtime.stop_requested))
    {
        struct pollfd fds[2];
        int rc;

        fds[0].fd = runtime.stop_pipe[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = runtime.keyboard_fd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        rc = poll(fds, 2, -1);
        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            stop_reason = "evdev poll failed.";
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            atomic_store(&runtime.stop_requested, true);
            stop_reason = "Stopped.";
            break;
        }

        if (!(fds[1].revents & POLLIN))
            continue;

        while (!atomic_load(&runtime.stop_requested))
        {
            struct input_event ev;
            ssize_t n = read(runtime.keyboard_fd, &ev, sizeof(ev));

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;
                stop_reason = "evdev read failed.";
                atomic_store(&runtime.stop_requested, true);
                break;
            }
            if (n != (ssize_t)sizeof(ev))
                continue;
            if (ev.type != EV_KEY)
                continue;

            if (ev.code == KEY_F12 && ev.value == 1)
            {
                emergency = true;
                atomic_store(&runtime.stop_requested, true);
                stop_reason = "Emergency stop (F12).";
                break;
            }

            if (ev.code != runtime.config.key_code)
                continue;

            if (ev.value == 1 && !c_held)
            {
                char error[256] = "";
                PollCancelContext cancel_ctx = {
                    .runtime = &runtime,
                    .c_released = false,
                    .emergency = false,
                };

                c_held = true;
                if (!camera_perform_turn(runtime.display,
                                         runtime.config.calibration,
                                         runtime.config.steps,
                                         runtime.config.step_delay_us,
                                         1,
                                         poll_cancel_func,
                                         &cancel_ctx,
                                         error,
                                         sizeof(error)))
                {
                    emergency = cancel_ctx.emergency;
                    stop_reason = emergency ? "Emergency stop (F12)." : "Stopped.";
                    atomic_store(&runtime.stop_requested, true);
                    break;
                }

                if (runtime.config.click_enabled)
                {
                    if (!camera_sleep_ms(runtime.config.click_delay_ms, poll_cancel_func, &cancel_ctx))
                    {
                        emergency = cancel_ctx.emergency;
                        stop_reason = emergency ? "Emergency stop (F12)." : "Stopped.";
                        atomic_store(&runtime.stop_requested, true);
                        break;
                    }
                    if (!camera_left_click(runtime.display, poll_cancel_func, &cancel_ctx, error, sizeof(error)))
                    {
                        emergency = cancel_ctx.emergency;
                        stop_reason = emergency ? "Emergency stop (F12)." : "Stopped.";
                        atomic_store(&runtime.stop_requested, true);
                        break;
                    }
                }

                if (cancel_ctx.c_released)
                {
                    c_held = false;
                    if (!camera_perform_turn(runtime.display,
                                             runtime.config.calibration,
                                             runtime.config.steps,
                                             runtime.config.step_delay_us,
                                             -1,
                                             poll_cancel_func,
                                             &cancel_ctx,
                                             error,
                                             sizeof(error)))
                    {
                        emergency = cancel_ctx.emergency;
                        stop_reason = emergency ? "Emergency stop (F12)." : "Stopped.";
                        atomic_store(&runtime.stop_requested, true);
                        break;
                    }
                }
            }
            else if (ev.value == 0 && c_held)
            {
                char error[256] = "";
                PollCancelContext cancel_ctx = {
                    .runtime = &runtime,
                    .c_released = false,
                    .emergency = false,
                };

                c_held = false;
                if (!camera_perform_turn(runtime.display,
                                         runtime.config.calibration,
                                         runtime.config.steps,
                                         runtime.config.step_delay_us,
                                         -1,
                                         poll_cancel_func,
                                         &cancel_ctx,
                                         error,
                                         sizeof(error)))
                {
                    emergency = cancel_ctx.emergency;
                    stop_reason = emergency ? "Emergency stop (F12)." : "Stopped.";
                    atomic_store(&runtime.stop_requested, true);
                    break;
                }
            }
        }
    }

    if (emergency)
        stop_reason = "Emergency stop (F12).";

    close_runtime_resources();

    pthread_mutex_lock(&runtime.mutex);
    runtime.running = false;
    pthread_mutex_unlock(&runtime.mutex);

    invoke_state_callback(false, stop_reason);
    return NULL;
}

static void join_finished_thread_if_needed(void)
{
    pthread_t thread;
    bool should_join = false;

    pthread_mutex_lock(&runtime.mutex);
    if (runtime.thread_valid && !runtime.running)
    {
        thread = runtime.thread;
        runtime.thread_valid = false;
        should_join = true;
    }
    pthread_mutex_unlock(&runtime.mutex);

    if (should_join)
        (void)pthread_join(thread, NULL);
}

bool input_start_macro(const AppConfig *config,
                       const MacroCallbacks *callbacks,
                       char *error,
                       size_t error_size)
{
    char device_path[PATH_MAX];
    char device_name[256];
    char key_name[64];
    int flags;

    if (!config)
    {
        input_set_error(error, error_size, "Missing configuration.");
        return false;
    }
    if (!is_bindable_keyboard_key(config->key_code))
    {
        input_set_error(error, error_size, "Invalid macro key. F12 is reserved for emergency stop.");
        return false;
    }

    join_finished_thread_if_needed();

    pthread_mutex_lock(&runtime.mutex);
    if (runtime.running)
    {
        pthread_mutex_unlock(&runtime.mutex);
        input_set_error(error, error_size, "Macro is already running.");
        return false;
    }
    pthread_mutex_unlock(&runtime.mutex);

    if (strcmp(config->keyboard_device, "auto") == 0)
    {
        if (!input_auto_detect_device_for_key(config->key_code, device_path, sizeof(device_path), device_name, sizeof(device_name)))
        {
            snprintf(error,
                     error_size,
                     "Could not auto-detect a readable keyboard device with %s.",
                     input_key_name(config->key_code, key_name, sizeof(key_name)));
            return false;
        }
    }
    else
    {
        snprintf(device_path, sizeof(device_path), "%s", config->keyboard_device);
    }

    runtime.keyboard_fd = open(device_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (runtime.keyboard_fd < 0)
    {
        snprintf(error, error_size, "Could not open %s: %s", device_path, strerror(errno));
        return false;
    }
    if (!device_supports_key(runtime.keyboard_fd, config->key_code))
    {
        close(runtime.keyboard_fd);
        runtime.keyboard_fd = -1;
        input_set_error(error, error_size, "Selected keyboard device does not support the configured key.");
        return false;
    }

    if (!camera_open_display(&runtime.display, error, error_size))
    {
        close(runtime.keyboard_fd);
        runtime.keyboard_fd = -1;
        return false;
    }
    if (!camera_xtest_available(runtime.display, NULL, NULL, error, error_size))
    {
        XCloseDisplay(runtime.display);
        runtime.display = NULL;
        close(runtime.keyboard_fd);
        runtime.keyboard_fd = -1;
        return false;
    }

    if (pipe(runtime.stop_pipe) != 0)
    {
        input_set_error(error, error_size, "Could not create stop pipe.");
        XCloseDisplay(runtime.display);
        runtime.display = NULL;
        close(runtime.keyboard_fd);
        runtime.keyboard_fd = -1;
        return false;
    }
    flags = fcntl(runtime.stop_pipe[0], F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(runtime.stop_pipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(runtime.stop_pipe[1], F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(runtime.stop_pipe[1], F_SETFL, flags | O_NONBLOCK);

    runtime.config = *config;
    if (strcmp(config->keyboard_device, "auto") == 0)
        snprintf(runtime.config.keyboard_device, sizeof(runtime.config.keyboard_device), "%s", device_path);
    runtime.callbacks = callbacks ? *callbacks : (MacroCallbacks){0};
    atomic_store(&runtime.stop_requested, false);

    if (pthread_create(&runtime.thread, NULL, input_thread_main, NULL) != 0)
    {
        close_runtime_resources();
        input_set_error(error, error_size, "Could not start input thread.");
        return false;
    }

    pthread_mutex_lock(&runtime.mutex);
    runtime.thread_valid = true;
    runtime.running = true;
    pthread_mutex_unlock(&runtime.mutex);

    invoke_state_callback(true, "Running.");
    return true;
}

void input_stop_macro(void)
{
    pthread_t thread;
    bool should_join = false;

    pthread_mutex_lock(&runtime.mutex);
    if (runtime.running)
    {
        atomic_store(&runtime.stop_requested, true);
        if (runtime.stop_pipe[1] >= 0)
        {
            const char byte = 'x';
            (void)write(runtime.stop_pipe[1], &byte, 1);
        }
    }
    if (runtime.thread_valid)
    {
        thread = runtime.thread;
        runtime.thread_valid = false;
        should_join = true;
    }
    pthread_mutex_unlock(&runtime.mutex);

    if (should_join)
        (void)pthread_join(thread, NULL);
}

bool input_is_running(void)
{
    bool is_running;

    pthread_mutex_lock(&runtime.mutex);
    is_running = runtime.running;
    pthread_mutex_unlock(&runtime.mutex);

    return is_running;
}

bool input_test_flip(const AppConfig *config, char *error, size_t error_size)
{
    Display *display = NULL;
    bool ok;

    if (input_is_running())
    {
        input_set_error(error, error_size, "Stop the macro before using Test Flip.");
        return false;
    }
    if (!camera_open_display(&display, error, error_size))
        return false;
    if (!camera_xtest_available(display, NULL, NULL, error, error_size))
    {
        XCloseDisplay(display);
        return false;
    }

    ok = camera_perform_turn(display, config->calibration, config->steps, config->step_delay_us,
                             1, NULL, NULL, error, error_size) &&
         camera_perform_turn(display, config->calibration, config->steps, config->step_delay_us,
                             -1, NULL, NULL, error, error_size);
    camera_release_buttons(display);
    XCloseDisplay(display);
    return ok;
}

bool input_test_click(char *error, size_t error_size)
{
    Display *display = NULL;
    bool ok;

    if (input_is_running())
    {
        input_set_error(error, error_size, "Stop the macro before using Test Click.");
        return false;
    }
    if (!camera_open_display(&display, error, error_size))
        return false;
    if (!camera_xtest_available(display, NULL, NULL, error, error_size))
    {
        XCloseDisplay(display);
        return false;
    }

    ok = camera_left_click(display, NULL, NULL, error, error_size);
    camera_release_buttons(display);
    XCloseDisplay(display);
    return ok;
}
