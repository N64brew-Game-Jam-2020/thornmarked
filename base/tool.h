// Library functions for writing tools.
#pragma once
#include "base/defs.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>

// Print an error message and exit.
noreturn void die_(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define die(...) die_(__FILE__, __LINE__, __VA_ARGS__)

// Print an error message with an errno error message and exit.
noreturn void die_errno_(const char *file, int line, int err, const char *fmt,
                         ...) __attribute__((format(printf, 4, 5)));

#define die_errno(err, ...) die_errno_(__FILE__, __LINE__, err, __VA_ARGS__)

// Print an error message for a failed read. This will either be an IO error or
// an unexpected EOF, depending on the state of ferror(fp) and feof(fp).
noreturn void die_read_(const char *file, int line, FILE *fp, const char *fmt,
                        ...) __attribute__((format(printf, 4, 5)));
#define die_read(fp, ...) die_read_(__FILE__, __LINE__, fp, __VA_ARGS__)

// Malloc, but exit the program on failure.
void *xmalloc(size_t size) __attribute__((malloc, alloc_size(1)));

// Malloc, but exit the program on failure.
void *xcalloc(size_t nmemb, size_t size)
    __attribute__((malloc, alloc_size(1, 2)));

// Convert to integer, abort on failure.
int xatoi(const char *s);

inline uint16_t swap16(uint16_t x) {
    return __builtin_bswap16(x);
}

inline uint32_t swap32(uint32_t x) {
    return __builtin_bswap32(x);
}

void swap16arr(int16_t *arr, size_t n);

// Pack two 16-bit values into a 32-bit value.
inline uint32_t pack32(uint16_t hi, uint16_t lo) {
    return ((uint32_t)hi << 16) | lo;
}
