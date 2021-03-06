#include "base/testlib/testlib.h"

#include "base/defs.h"

// =============================================================================
// Nintendo 64 Test Harness
// =============================================================================

#if _ULTRA64

#include "base/console.h"
#include "base/n64/console.h"
#include "base/n64/os.h"

#include <ultra64.h>

enum {
    SCREEN_WIDTH = 320,
    SCREEN_HEIGHT = 240,
};

static uint16_t framebuffers[2][SCREEN_WIDTH * SCREEN_HEIGHT]
    __attribute__((section("uninit.cfb"), aligned(16)));

static void test_show(uint16_t *framebuffer) {
    console_draw_raw(&console, framebuffer);
    osWritebackDCache(framebuffer, sizeof(framebuffers[0]));
    osViSwapBuffer(framebuffer);
}

void test_start(const char *name) {
    console_init(&console, CONSOLE_TRUNCATE);
    console_printf(&console, "Test: %s\n", name);
}

void test_logf_func(const char *file, int line, const char *fmt, ...) {
    console_printf(&console, "%s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    console_vprintf(&console, fmt, ap);
    va_end(ap);
    console_putc(&console, '\n');
}

noreturn void test_fail_func(const char *file, int line) {
    console_printf(&console, "%s:%d: FAILED", file, line);
    test_show(framebuffers[1]);
    for (;;) {}
}

static void main(void *arg);

extern u8 _main_thread_stack[];

void boot(void) {
    system_main(main, NULL, _main_thread_stack);
}

static void main(void *arg) {
    (void)arg;

    // Setup.
    console_init(&console, CONSOLE_TRUNCATE);
    console_puts(&console, "Running tests...");
    test_show(framebuffers[0]);
    osViBlack(false);

    // Run test.
    test_main();

    // Print OK message.
    console_init(&console, CONSOLE_TRUNCATE);
    console_puts(&console, "OK");
    test_show(framebuffers[1]);

    // Done.
    for (;;) {}
}

// =============================================================================
// PC Test Harness
// =============================================================================

#else

#include "base/console.h"
#include "base/system_pc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void test_start(const char *name) {
    fprintf(stderr, "\nTest: %s\n", name);
}

void test_logf_func(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "%s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

noreturn void test_fail_func(const char *file, int line) {
    fprintf(stderr, "%s:%d: FAILED\n", file, line);
    exit(1);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    console_vfatal_func = console_vfatal_impl;
    test_main();
    fputs("OK\n", stderr);
    return 0;
}

#endif
