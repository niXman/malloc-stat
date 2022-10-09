/*
 * malloc-stat
 * Alloca/free library with backtrace for not freed areas and byte-exact memory tracking.
 * Author: niXman
 * https://github.com/niXman/malloc-stat
 *
 * the status can be obtained by using:
 * void malloc_stat_get_stat(uint64_t *allocations, uint64_t *deallocations, uint64_t *in_use),
 * or just by using MACROs from api.h
 *
 * Based on previous implementations by other authors. See their comments below:
 */
/*
 * log-malloc-simple
 *	Malloc logging library with backtrace and byte-exact memory tracking.
 * 
 * Author: András Schmidt
 * https://github.com/rizsi
 *
 * Based on previous implementations by other authors. See their comments below:
 */ 
/*
 * Based on Author: Samuel Behan <_samuel_._behan_(at)_dob_._sk> (C) 2011-2014
 *	   partialy based on log-malloc from Ivan Tikhonov
 *
 * License: GNU LGPLv3 (http://www.gnu.org/licenses/lgpl.html)
 *
 * Web:
 *	http://devel.dob.sk/log-malloc2
 *	http://blog.dob.sk/category/devel/log-malloc2 (howto, tutorials)
 *	https://github.com/samsk/log-malloc2 (git repo)
 *
 */

/* Copyright (C) 2007 Ivan Tikhonov
   Ivan Tikhonov, http://www.brokestream.com, kefeer@netangels.ru

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Ivan Tikhonov, kefeer@brokestream.com

*/

/* needed for dlfcn.h */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <malloc.h>
#include <dlfcn.h>

#include <malloc-stat/api.h>

/* config */
/** Maximum bytes of a single log entry. They are prepared in buffers of this size allocated on the stack.  */
#define LOG_BUFSIZE 4096
/** FD where output is written to. */
#define LOG_MALLOC_TRACE_FD 1022

/* init constants */
#define LOG_MALLOC_INIT_NULL    0xFAB321
#define LOG_MALLOC_INIT_STARTED 0xABCABC
#define LOG_MALLOC_INIT_DONE    0x123FAB
#define LOG_MALLOC_FINI_DONE    0xFAFBFC

/* handler declarations */
static void *(*real_malloc)(size_t size) = NULL;
static void  (*real_free)(void *ptr) = NULL;
static void *(*real_realloc)(void *ptr, size_t size) = NULL;
static void *(*real_calloc)(size_t nmemb, size_t size) = NULL;
static void *(*real_memalign)(size_t boundary, size_t size) = NULL;
static int   (*real_posix_memalign)(void **memptr, size_t alignment, size_t size) = NULL;
static void *(*real_valloc)(size_t size) = NULL;
static void *(*real_pvalloc)(size_t size) = NULL;
static void *(*real_aligned_alloc)(size_t alignment, size_t size) = NULL;

/* DL resolving */
#define DL_RESOLVE(fn) \
    ((!real_ ## fn) ? (real_ ## fn = dlsym(RTLD_NEXT, # fn)) : (real_ ## fn = ((void *)0x1)))

#define DL_RESOLVE_CHECK(fn) \
    ((!real_ ## fn) ? malloc_stat_init_lib() : 1)

/* Flag that stores initialization state */
static sig_atomic_t init_done = LOG_MALLOC_INIT_NULL;
/* output is disabled because the lineno does not exist */
static bool memlog_disabled = false;

/// On this thread we are currently writing a trace event so prevent self-recursion
static __thread int in_trace = 0;

/*
 *  INTERNAL API FUNCTIONS
 */

/** Count the length of a null terminated string. */
int myStrlen(const char * str) {
    int ret=0;
    for ( ; *str; ++str, ++ret )
    {}

    return ret;
}
/*
 *  LIBRARY INIT/FINI FUNCTIONS
 */
/**
 * Copy data from file to the log.
 */
static inline void copyfile(const char *head, const char *path, int outfd) {
    int fd = -1;
    char buf[BUFSIZ];
    ssize_t len = 0;
    size_t head_len = myStrlen(head);
    // no warning here, it will be simply missing in log
    if ( (fd = open(path, 0)) == -1 ) {
        return;
    }

    write(outfd, head, head_len);
    // ignoring EINTR here, use SA_RESTART to fix if problem
    while ( (len = read(fd, buf, sizeof(buf))) > 0 ) {
        write(outfd, buf, len);
    }
    close(fd);

    return;
}

static inline void write_log(const char * buf, int size) {
    if ( !memlog_disabled ) {
        write(LOG_MALLOC_TRACE_FD, buf, size);
    }
}

/** During initialization while the real_* pointer are being set up we can not pass malloc calls to the real
 * malloc.
 * During this time malloc calls allocate memory in this area.
 * After initialization is done this buffer is no longer used.
 */
#define STATIC_SIZE 1024
static char static_buffer[STATIC_SIZE];
static int static_pointer = 0;

static uint64_t total_allocations = 0;
static uint64_t total_deallocations = 0;
static uint64_t total_in_use = 0;

static void malloc_stat_get_stat(uint64_t *allocations, uint64_t *deallocations, uint64_t *in_use) {
    /* just a compile-tile test for lock-free are available */
    char _[__atomic_always_lock_free(sizeof(*allocations), allocations) ? 1 : -1]; (void)_;

    *allocations   = __atomic_load_n(&total_allocations, __ATOMIC_SEQ_CST);
    *deallocations = __atomic_load_n(&total_deallocations, __ATOMIC_SEQ_CST);
    *in_use        = __atomic_load_n(&total_in_use, __ATOMIC_SEQ_CST);
}

static inline void log_mem(const char * method, void *ptr, size_t size) {
    /* Prevent preparing the output in memory in case the output is already closed */
    if ( !memlog_disabled ) {
        char buf[LOG_BUFSIZE];
        int len = snprintf(buf, sizeof(buf), "+ %s %zu %p %d %d\n", method,
            size, ptr, getpid(), gettid());

        len += snprintf(buf+len, sizeof(buf)-len, "-\n");
        write_log(buf, len);
    }
    return;
}



int malloc_stat_init_lib(void) {
    /* check already initialized */
    if ( !__sync_bool_compare_and_swap(&init_done,
        LOG_MALLOC_INIT_NULL, LOG_MALLOC_INIT_STARTED) )
    {
        return 1;
    }

    int w = write(LOG_MALLOC_TRACE_FD, "INIT\n", 5);
    /* auto-disable trace if file is not open  */
    if ( w == -1 && errno == EBADF ) {
        write(STDERR_FILENO, "1022_CLOSE\n", 11);
        memlog_disabled = true;
    } else {
        write(STDERR_FILENO, "1022_OPEN\n", 10);
        memlog_disabled = false;
    }

    /* get real functions pointers */
    DL_RESOLVE(malloc);
    DL_RESOLVE(calloc);
    DL_RESOLVE(free);
    DL_RESOLVE(realloc);
    DL_RESOLVE(memalign);
    DL_RESOLVE(posix_memalign);
    DL_RESOLVE(valloc);
    DL_RESOLVE(pvalloc);
    DL_RESOLVE(aligned_alloc);

    __sync_bool_compare_and_swap(&init_done,
        LOG_MALLOC_INIT_STARTED, LOG_MALLOC_INIT_DONE);

    //TODO: call backtrace here to init itself

    /* post-init status */
    if( !memlog_disabled ) {
        int s;
        char path[256];
        char buf[LOG_BUFSIZE + sizeof(path)];

        s = snprintf(buf, sizeof(buf), "# PID %u\n", getpid());
        write_log(buf, s);

        s = readlink("/proc/self/exe", path, sizeof(path));
        if ( s > 1 ) {
            path[s] = '\0';
            s = snprintf(buf, sizeof(buf), "# EXE %s\n", path);
            write_log(buf, s);
        }

        s = readlink("/proc/self/cwd", path, sizeof(path));
        if ( s > 1 ) {
            path[s] = '\0';
            s = snprintf(buf, sizeof(buf), "# CWD %s\n", path);
            write_log(buf, s);
        }
        copyfile("# MAPS\n", "/proc/self/maps", LOG_MALLOC_TRACE_FD);
        /*
        s = readlink("/proc/self/maps", path, sizeof(path));
        if(s > 1)
        {
        path[s] = '\0';
        s = snprintf(buf, sizeof(buf), "# MAPS %s\n", path);
        write_log(buf, s);
        }
        */

        s = snprintf(buf, sizeof(buf), "+ INIT \n-\n");
        log_mem("INIT", &static_buffer, static_pointer);
        // write_log(buf, s);
    }

    malloc_stat_fnptr_received(malloc_stat_get_stat);

    return 0;
}

void malloc_stat_fini_lib(void) {
    /* check already finalized */
    if ( !__sync_bool_compare_and_swap(&init_done,
        LOG_MALLOC_INIT_DONE, LOG_MALLOC_FINI_DONE) ) {
        return;
    }

    if ( !memlog_disabled ) {
        int s;
        char buf[LOG_BUFSIZE];
        // const char maps_head[] = "# FILE /proc/self/maps\n";

        s = snprintf(buf, sizeof(buf), "+ FINI\n-\n");
        write_log(buf, s);

        /* maps out here, because dynamic libs could by mapped during run */
        //copyfile(maps_head, sizeof(maps_head) - 1, g_maps_path, g_ctx.memlog_fd);
    }

    return;
}

static void __attribute__ ((constructor))malloc_stat_init(void) {
    malloc_stat_init_lib();
    return;
}

static void __attribute__ ((destructor))malloc_stat_fini(void) {
    malloc_stat_fini_lib();
    return;
}

/*
 *  INTERNAL FUNCTIONS
 */

static inline void * calloc_static(size_t nmemb, size_t size) {
    void *ret = static_buffer + static_pointer;
    static_pointer += size * nmemb;

    if ( static_pointer > STATIC_SIZE ) {
        return NULL;
    }

    size_t i;
    for ( i = 0 ; i < size; ++i ) {
        *(((char *)ret) + i) = 0;
    }

    return ret;
}

/*
 *  LIBRARY FUNCTIONS
 */

void* malloc(size_t size) {
    if ( !DL_RESOLVE_CHECK(malloc) ) {
        return calloc_static(size, 1);
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_malloc(size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("malloc", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void* calloc(size_t nmemb, size_t size) {
    if ( !DL_RESOLVE_CHECK(calloc) ) {
        return calloc_static(nmemb, size);
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_calloc(nmemb, size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("calloc", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void* realloc(void *ptr, size_t size) {
    if ( !DL_RESOLVE_CHECK(realloc) ) {
        return NULL;
    }

    /* no need to decrement/increment total_allocations in this case */
    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    size_t prevSize = malloc_usable_size(ptr);
    __atomic_sub_fetch(&total_in_use, prevSize, __ATOMIC_RELAXED);

    void *ret = real_realloc(ptr, size);
    size_t afterSize = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, afterSize, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        if ( ptr ) {
            log_mem("realloc_free", ptr, prevSize);
        }

        log_mem("realloc_alloc", ret, afterSize);
        in_trace = 0;
    }

    return ret;
}

void* memalign(size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(memalign) ) {
        return NULL;
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_memalign(alignment, size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("memalign", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(posix_memalign) ) {
        return ENOMEM;
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    int ret = real_posix_memalign(memptr, alignment, size);
    size_t sizeAllocated = malloc_usable_size(*memptr);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("posix_memalign", *memptr, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void* valloc(size_t size) {
    if ( !DL_RESOLVE_CHECK(valloc) ) {
       return NULL;
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_valloc(size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("valloc", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void* pvalloc(size_t size) {
    if( !DL_RESOLVE_CHECK(pvalloc) ) {
        return NULL;
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_pvalloc(size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("pvalloc", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(aligned_alloc) ) {
        return NULL;
    }

    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED);

    void *ret = real_aligned_alloc(alignment, size);
    size_t sizeAllocated = malloc_usable_size(ret);

    __atomic_add_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

    if ( !in_trace ) {
        in_trace = 1;
        log_mem("aligned_alloc", ret, sizeAllocated);
        in_trace = 0;
    }

    return ret;
}

void free(void *ptr) {
    if ( !DL_RESOLVE_CHECK(free) ) {
        // We can not log anything here because the log message would result another free call and it would fall into an endless loop
        return;
    }

    size_t sizeAllocated = 0;
    if ( ptr ) {
        __atomic_add_fetch(&total_deallocations, 1, __ATOMIC_RELAXED);

        size_t sizeAllocated = malloc_usable_size(ptr);
        __atomic_sub_fetch(&total_in_use, sizeAllocated, __ATOMIC_RELAXED);

        real_free(ptr);
    }

    if ( !in_trace ) {
        in_trace = 1;
        log_mem((ptr ? "free" : "free(NULL)"), ptr, sizeAllocated);
        in_trace = 0;
    }
}

/* EOF */
