#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define KEYBOARD_DEVICE "/dev/input/event10"
#define TARGET_KEY KEY_C

/*
 * ============================================================
 * CAMERA CALIBRATION
 * ============================================================
 *
 * Total horizontal relative movement for one 180-degree turn.
 *
 * Tune this value if necessary.
 */
#define TURN_180_TOTAL 2500

/*
 * Number of relative-movement events used for the turn.
 */
#define TURN_STEPS 32

/*
 * Microseconds between movement events.
 *
 * 500 = 0.5 ms
 */
#define STEP_DELAY_US 500


/*
 * ============================================================
 * CLICK TIMING
 * ============================================================
 *
 * Delay AFTER the 180-degree flip has completely finished
 * and BEFORE the left click occurs.
 *
 * Examples:
 *
 * 0   = click immediately
 * 1   = 1 ms
 * 5   = 5 ms
 * 10  = 10 ms
 * 25  = 25 ms
 * 50  = 50 ms
 */
#define CLICK_DELAY_MS 1


/*
 * ============================================================
 * GLOBALS
 * ============================================================
 */

static volatile sig_atomic_t running = 1;

static Display *display = NULL;


/*
 * ============================================================
 * SIGNAL HANDLING
 * ============================================================
 */

static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}


/*
 * ============================================================
 * HIGH-RESOLUTION SLEEP
 * ============================================================
 */

static void sleep_us(long us)
{
    struct timespec ts;

    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000L;

    nanosleep(&ts, NULL);
}


/*
 * ============================================================
 * RELATIVE X MOVEMENT
 * ============================================================
 */

static int move_relative(int dx)
{
    if (!display)
        return -1;

    if (!XTestFakeRelativeMotionEvent(
            display,
            dx,
            0,
            CurrentTime))
    {
        return -1;
    }

    XFlush(display);

    return 0;
}


/*
 * ============================================================
 * LEFT CLICK
 * ============================================================
 *
 * Button 1 = left mouse button.
 */

static int left_click(void)
{
    if (!display)
        return -1;

    /*
     * Press.
     */
    if (!XTestFakeButtonEvent(
            display,
            1,
            True,
            CurrentTime))
    {
        return -1;
    }

    XFlush(display);

    /*
     * Very short physical-like button hold.
     */
    sleep_us(1000);

    /*
     * Release.
     */
    if (!XTestFakeButtonEvent(
            display,
            1,
            False,
            CurrentTime))
    {
        return -1;
    }

    XFlush(display);

    return 0;
}


/*
 * ============================================================
 * CAMERA TURN
 * ============================================================
 */

static int perform_turn(int direction)
{
    int base;
    int remainder;

    base = TURN_180_TOTAL / TURN_STEPS;
    remainder = TURN_180_TOTAL % TURN_STEPS;

    for (int i = 0; i < TURN_STEPS; ++i)
    {
        int amount = base;

        if (i < remainder)
            ++amount;

        amount *= direction;

        /*
         * X movement ONLY.
         *
         * Y is always zero.
         */
        if (move_relative(amount) < 0)
        {
            fprintf(
                stderr,
                "XTest relative motion failed.\n"
            );

            return -1;
        }

        sleep_us(STEP_DELAY_US);
    }

    /*
     * Make sure all X11 requests are sent before returning.
     */
    XFlush(display);

    return 0;
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */

int main(void)
{
    int keyboard_fd;
    bool c_held = false;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf(
        "\n"
        "====================================\n"
        " Roblox Camera Flip Macro\n"
        "====================================\n\n"
    );

    printf(
        "Keyboard: %s\n",
        KEYBOARD_DEVICE
    );

    printf(
        "C key: KEY_C (%d)\n",
        TARGET_KEY
    );

    printf(
        "180-degree calibration: %d\n",
        TURN_180_TOTAL
    );

    printf(
        "Steps: %d\n",
        TURN_STEPS
    );

    printf(
        "Step delay: %d us\n",
        STEP_DELAY_US
    );

    printf(
        "Click delay: %d ms\n\n",
        CLICK_DELAY_MS
    );


    /*
     * ========================================================
     * OPEN X11
     * ========================================================
     */

    display = XOpenDisplay(NULL);

    if (!display)
    {
        fprintf(
            stderr,
            "Could not open X11 display.\n"
        );

        return 1;
    }


    /*
     * ========================================================
     * CHECK XTEST
     * ========================================================
     */

    int event_base;
    int error_base;
    int major = 2;
    int minor = 2;

    if (!XTestQueryExtension(
            display,
            &event_base,
            &error_base,
            &major,
            &minor))
    {
        fprintf(
            stderr,
            "XTest extension is not available.\n"
        );

        XCloseDisplay(display);

        return 1;
    }

    printf(
        "XTest: available (version %d.%d)\n",
        major,
        minor
    );


    /*
     * ========================================================
     * OPEN REAL KEYBOARD
     * ========================================================
     */

    printf(
        "Opening keyboard...\n"
    );

    keyboard_fd = open(
        KEYBOARD_DEVICE,
        O_RDONLY
    );

    if (keyboard_fd < 0)
    {
        perror(KEYBOARD_DEVICE);

        XCloseDisplay(display);

        return 1;
    }


    /*
     * ========================================================
     * READY
     * ========================================================
     */

    printf(
        "\n"
        "READY.\n"
        "\n"
        "Hold C  = flip + left click\n"
        "Release = return 180 degrees\n"
        "\n"
        "Ctrl+C = exit\n"
        "\n"
    );


    /*
     * ========================================================
     * INPUT LOOP
     * ========================================================
     */

    while (running)
    {
        struct input_event ev;

        ssize_t n = read(
            keyboard_fd,
            &ev,
            sizeof(ev)
        );

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            perror("keyboard read");

            break;
        }

        if (n != sizeof(ev))
            continue;

        if (ev.type != EV_KEY)
            continue;

        if (ev.code != TARGET_KEY)
            continue;


        /*
         * ====================================================
         * C PRESSED
         * ====================================================
         */

        if (
            ev.value == 1 &&
            !c_held
        )
        {
            c_held = true;

            printf(
                "C DOWN -> FLIP\n"
            );

            fflush(stdout);


            /*
             * First perform the complete 180-degree turn.
             */
            if (perform_turn(+1) < 0)
                break;


            /*
             * Then wait the configured amount of time.
             */
            if (CLICK_DELAY_MS > 0)
            {
                sleep_us(
                    CLICK_DELAY_MS * 1000L
                );
            }


            /*
             * Finally perform left click.
             */
            printf(
                "CLICK\n"
            );

            fflush(stdout);

            if (left_click() < 0)
            {
                fprintf(
                    stderr,
                    "Left click injection failed.\n"
                );

                break;
            }
        }


        /*
         * ====================================================
         * C RELEASED
         * ====================================================
         */

        else if (
            ev.value == 0 &&
            c_held
        )
        {
            c_held = false;

            printf(
                "C UP -> RETURN\n"
            );

            fflush(stdout);


            /*
             * Return exactly the opposite amount.
             */
            if (perform_turn(-1) < 0)
                break;
        }
    }


    /*
     * ========================================================
     * CLEANUP
     * ========================================================
     */

    close(keyboard_fd);

    XCloseDisplay(display);

    printf(
        "\nMacro stopped.\n"
    );

    return 0;
}