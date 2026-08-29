/*
 * Compatibility wrapper for one-file builds.
 *
 * The maintained Curve Macro GUI application source now lives in src/.
 * Prefer:
 *
 *     make
 *
 * If you need a single-translation-unit build, compile this file with the
 * same GTK/X11/XTest flags used by the Makefile.
 */

#include "src/config.c"
#include "src/performance.c"
#include "src/camera.c"
#include "src/input.c"
#include "src/gui.c"
#include "src/main.c"
