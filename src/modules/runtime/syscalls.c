/*!
    \file    syscall.c
    \brief   system calls file

    \version 2026-01-31, V1.4.0, firmware for GD32F527
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <reent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#include "modules/runtime/heap.h"

int syscalls_gettimeofday_port(struct timeval *tv, struct timezone *tz);

#undef errno
extern int errno;

__attribute__((weak)) int syscalls_write_port(char *buf, uint32_t len)
{
    (void)(buf);
    (void)(len);
    return -1;
}

__attribute__((weak)) int syscalls_read_port()
{
    return 0;
}

void *_sbrk(ptrdiff_t incr)
{
    (void)incr;
    errno = ENOMEM;
    return (void *)-1;
}

void *__wrap__malloc_r(struct _reent *reent, size_t nbytes)
{
    void *ptr = heap_alloc(nbytes);
    if ((ptr == NULL) && (reent != NULL)) {
        reent->_errno = ENOMEM;
    }
    return ptr;
}

void __wrap__free_r(struct _reent *reent, void *aptr)
{
    (void)reent;
    heap_free(aptr);
}

void *__wrap__calloc_r(struct _reent *reent, size_t n, size_t size)
{
    size_t total;

    if ((n != 0U) && __builtin_mul_overflow(n, size, &total)) {
        if (reent != NULL) {
            reent->_errno = ENOMEM;
        }
        return NULL;
    }

    void *ptr = heap_alloc(total);
    if (ptr == NULL) {
        if (reent != NULL) {
            reent->_errno = ENOMEM;
        }
        return NULL;
    }

    memset(ptr, 0, total);
    return ptr;
}

void *__wrap__realloc_r(struct _reent *reent, void *aptr, size_t nbytes)
{
    void *ptr = heap_realloc(aptr, nbytes);
    if ((ptr == NULL) && (nbytes != 0U) && (reent != NULL)) {
        reent->_errno = ENOMEM;
    }
    return ptr;
}

/* _gettimeofday: RTC wall clock via modules::system */
int _gettimeofday(struct timeval *tp, struct timezone *tzp)
{
    return syscalls_gettimeofday_port(tp, tzp);
}

void initialise_monitor_handles() {}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    _kill(status, -1);
    while (1) {
    }
}

int _write(int file, char *ptr, int len)
{
    /* stdout / stderr only */
    if ((len < 0) || ((len > 0) && (ptr == NULL)) || ((file != 1) && (file != 2))) {
        errno = EINVAL;
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    const char last = ptr[len - 1];
    const int lone_lf = (last == '\n') && ((len == 1) || (ptr[len - 2] != '\r'));
    const int lone_cr = (last == '\r');

    if (lone_lf) {
        if (len > 1) {
            if (syscalls_write_port(ptr, (uint32_t)(len - 1)) < 0) {
                errno = EIO;
                return -1;
            }
        }
        char crlf[2] = {'\r', '\n'};
        if (syscalls_write_port(crlf, 2) < 0) {
            errno = EIO;
            return -1;
        }
        return len;
    }

    if (syscalls_write_port(ptr, len) < 0) {
        errno = EIO;
        return -1;
    }

    if (lone_cr) {
        char nl = '\n';
        if (syscalls_write_port(&nl, 1) < 0) {
            errno = EIO;
            return -1;
        }
    }

    return len;
}

int _close(int file)
{
    return -1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)(file);
    (void)(ptr);
    (void)(len);
    errno = EIO;
    return (syscalls_read_port() > 0) ? 1 : -1;
}

int _open(char *path, int flags, ...)
{
    /* Pretend like we always fail */
    return -1;
}

int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}

int _times(struct tms *buf)
{
    return -1;
}

int _stat(char *file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

int _fork(void)
{
    errno = EAGAIN;
    return -1;
}

int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}

void _init(void) {}

void _fini(void) {}
