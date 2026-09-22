#ifndef MODULES_CONSOLE_H
#define MODULES_CONSOLE_H

#include <stdint.h>

typedef int (*console_write_backend)(const uint8_t *data, uint32_t size);

int console_init(console_write_backend write_backend);
int console_write(const char *data, uint32_t size);
int console_puts(const char *text);
/* %s %c %d %u %ld %lu %x %% only. No width, precision, or float. */
int console_print(const char *fmt, ...);

#endif
