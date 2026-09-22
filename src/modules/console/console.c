#include "modules/console/console.h"

#include <stdarg.h>
#include "tx_api.h"

static TX_MUTEX console_lock;
static console_write_backend backend;
static uint8_t console_ready;

static uint32_t cstr_len(const char *text)
{
    uint32_t n = 0U;
    while (text[n] != '\0')
        ++n;
    return n;
}

static int emit(const char *data, uint32_t size)
{
    return backend((const uint8_t *)data, size) == (int)size ? 0 : -1;
}

static int emit_uint(uint32_t value, uint32_t base)
{
    char buf[10];
    uint32_t n = 0U;
    const char *digits = "0123456789abcdef";

    do {
        buf[n++] = digits[value % base];
        value /= base;
    } while (value != 0U);

    while (n != 0U) {
        char ch = buf[--n];
        if (emit(&ch, 1U) < 0)
            return -1;
    }
    return 0;
}

static int locked_write(const char *data, uint32_t size)
{
    int status;
    if (console_ready == 0U || (data == 0 && size != 0U))
        return -1;
    (void)tx_mutex_get(&console_lock, TX_WAIT_FOREVER);
    status = emit(data, size);
    (void)tx_mutex_put(&console_lock);
    return status == 0 ? (int)size : -1;
}

int console_init(console_write_backend write_backend)
{
    if (console_ready != 0U)
        return 0;
    if (write_backend == 0)
        return -1;
    if (tx_mutex_create(&console_lock, "console", TX_INHERIT) != TX_SUCCESS)
        return -1;
    backend = write_backend;
    console_ready = 1U;
    return 0;
}

int console_write(const char *data, uint32_t size)
{
    return locked_write(data, size);
}

int console_puts(const char *text)
{
    if (text == 0)
        return -1;
    if (locked_write(text, cstr_len(text)) < 0)
        return -1;
    return locked_write("\r\n", 2U) < 0 ? -1 : 0;
}

int console_print(const char *fmt, ...)
{
    va_list args;
    const char *p;
    int status = -1;

    if (fmt == 0 || console_ready == 0U)
        return -1;

    va_start(args, fmt);
    (void)tx_mutex_get(&console_lock, TX_WAIT_FOREVER);
    for (p = fmt; *p != '\0'; ++p) {
        if (*p != '%') {
            if (emit(p, 1U) < 0)
                goto done;
            continue;
        }
        ++p;
        if (*p == 'l' && (p[1] == 'u' || p[1] == 'd')) {
            char spec = p[1];
            ++p;
            if (spec == 'u') {
                if (emit_uint((uint32_t)va_arg(args, unsigned long), 10U) < 0)
                    goto done;
            } else {
                long v = va_arg(args, long);
                uint32_t mag;
                if (v < 0) {
                    char minus = '-';
                    if (emit(&minus, 1U) < 0)
                        goto done;
                    mag = 0U - (uint32_t)v;
                } else {
                    mag = (uint32_t)v;
                }
                if (emit_uint(mag, 10U) < 0)
                    goto done;
            }
        } else if (*p == 'd') {
            int v = va_arg(args, int);
            uint32_t mag;
            if (v < 0) {
                char minus = '-';
                if (emit(&minus, 1U) < 0)
                    goto done;
                mag = 0U - (uint32_t)v;
            } else {
                mag = (uint32_t)v;
            }
            if (emit_uint(mag, 10U) < 0)
                goto done;
        } else if (*p == 'u') {
            if (emit_uint(va_arg(args, unsigned int), 10U) < 0)
                goto done;
        } else if (*p == 'x') {
            if (emit_uint(va_arg(args, unsigned int), 16U) < 0)
                goto done;
        } else if (*p == 's') {
            const char *s = va_arg(args, const char *);
            if (s == 0)
                s = "(null)";
            if (emit(s, cstr_len(s)) < 0)
                goto done;
        } else if (*p == 'c') {
            char ch = (char)va_arg(args, int);
            if (emit(&ch, 1U) < 0)
                goto done;
        } else if (*p == '%') {
            if (emit(p, 1U) < 0)
                goto done;
        } else if (*p == '\0') {
            break;
        } else if (emit(p, 1U) < 0) {
            goto done;
        }
    }
    status = 0;

done:
    (void)tx_mutex_put(&console_lock);
    va_end(args);
    return status;
}

int syscalls_write_port(char *buf, uint32_t len)
{
    return locked_write(buf, len);
}

int syscalls_read_port(void)
{
    return 0;
}
