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
static int memlog_disabled = false;

/* log output fd */
static int memlog_fd = LOG_MALLOC_TRACE_FD;

/* On this thread we are currently writing a trace event so prevent self-recursion */
static __thread int in_trace = 0;

#define MALLOC_STAT_TRACE(caption, ptr, size) { \
    if ( !in_trace ) { \
        in_trace = 1; \
        log_mem(caption, ptr, size); \
        in_trace = 0; \
    } \
}

#define MALLOC_STAT_WRITE_LOG(ptr, size) \
    (!memlog_disabled ? write(memlog_fd, buf, size) : 0)

static inline void log_mem(const char *method, void *ptr, size_t size) {
    /* Prevent preparing the output in memory in case the output is already closed */
    if ( !memlog_disabled ) {
        char buf[LOG_BUFSIZE];
        int len = snprintf(
             buf
            ,sizeof(buf)
            ,"+ %s %zu %p %d %d\n"
            ,method
            ,size
            ,ptr
            ,getpid()
            ,gettid()
        );
        MALLOC_STAT_WRITE_LOG(buf, len);
    }

    return;
}

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

/* During initialization while the real_* pointer are being set up we can not pass malloc calls to the real
 * malloc.
 * During this time malloc calls allocate memory in this area.
 * After initialization is done this buffer is no longer used.
 */
#define STATIC_SIZE 1024
static char static_buffer[STATIC_SIZE];
static int static_pointer = 0;

/* stat variables */
static uint64_t total_allocations = 0;
static uint64_t total_allocated = 0;
static uint64_t total_deallocations = 0;
static uint64_t total_deallocated = 0;
static uint64_t total_in_use = 0;
static uint64_t peak_in_use = 0;

/* helpers */
#define MALLOC_STAT_INC_ALLOCATIONS() \
    __atomic_add_fetch(&total_allocations, 1, __ATOMIC_RELAXED)

#define MALLOC_STAT_INC_DEALLOCATIONS() \
    __atomic_add_fetch(&total_deallocations, 1, __ATOMIC_RELAXED)

#define MALLOC_STAT_ADD_ALLOCATED(size) \
    __atomic_add_fetch(&total_allocated, size, __ATOMIC_RELAXED)

#define MALLOC_STAT_ADD_DEALLOCATED(size) \
    __atomic_add_fetch(&total_deallocated, size, __ATOMIC_RELAXED)

#define MALLOC_STAT_ADD_IN_USE(size) \
    __atomic_add_fetch(&total_in_use, size, __ATOMIC_RELAXED)

#define MALLOC_STAT_SUB_IN_USE(size) \
    __atomic_sub_fetch(&total_in_use, size, __ATOMIC_RELAXED)

/* stat routine */
malloc_stat_vars malloc_stat_get_stat(malloc_stat_operation op) {
    malloc_stat_vars res;

    /* just a compile-time test for lock-free ops on uint64_t */
    char _[__atomic_always_lock_free(sizeof(res.allocations), &(res.allocations)) ? 1 : -1]; (void)_;

    switch ( op ) {
        case MALLOC_STAT_GET: break;
        case MALLOC_STAT_RESET: {
            __atomic_store_n(&total_allocations, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&total_allocated, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&total_deallocations, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&total_deallocated, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&total_in_use, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&peak_in_use, 0, __ATOMIC_RELAXED);
        } break;
    }

    res.allocations   = __atomic_load_n(&total_allocations, __ATOMIC_SEQ_CST);
    res.allocated     = __atomic_load_n(&total_allocated, __ATOMIC_SEQ_CST);
    res.deallocations = __atomic_load_n(&total_deallocations, __ATOMIC_SEQ_CST);
    res.deallocated   = __atomic_load_n(&total_deallocated, __ATOMIC_SEQ_CST);
    res.in_use        = __atomic_load_n(&total_in_use, __ATOMIC_SEQ_CST);
    res.peak_in_use   = __atomic_load_n(&peak_in_use, __ATOMIC_SEQ_CST);

    return res;
}

void malloc_stat_change_log_state(int op) {
    memlog_disabled = !op;
}

void malloc_stat_set_log_fd(int fd) {
    memlog_fd = fd;
}

/*
 *  LIBRARY INIT/FINI FUNCTIONS
 */

int malloc_stat_init_lib(void) {
    /* check already initialized */
    if ( !__sync_bool_compare_and_swap(&init_done,
        LOG_MALLOC_INIT_NULL, LOG_MALLOC_INIT_STARTED) )
    {
        return 1;
    }

    if ( !memlog_disabled ) {
        int w = write(memlog_fd, "INIT\n", 5);
        /* auto-disable trace if file is not open  */
        if ( w == -1 && errno == EBADF ) {
            write(STDERR_FILENO, "1022_CLOSED\n", 12);
            memlog_disabled = true;
        } else {
            write(STDERR_FILENO, "1022_OPEN\n", 10);
            memlog_disabled = false;
        }
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
        MALLOC_STAT_WRITE_LOG(buf, s);

        s = readlink("/proc/self/exe", path, sizeof(path));
        if ( s > 1 ) {
            path[s] = '\0';
            s = snprintf(buf, sizeof(buf), "# EXE %s\n", path);
            MALLOC_STAT_WRITE_LOG(buf, s);
        }

        s = readlink("/proc/self/cwd", path, sizeof(path));
        if ( s > 1 ) {
            path[s] = '\0';
            s = snprintf(buf, sizeof(buf), "# CWD %s\n", path);
            MALLOC_STAT_WRITE_LOG(buf, s);
        }
        copyfile("# MAPS\n", "/proc/self/maps", memlog_fd);
        /*
        s = readlink("/proc/self/maps", path, sizeof(path));
        if(s > 1)
        {
        path[s] = '\0';
        s = snprintf(buf, sizeof(buf), "# MAPS %s\n", path);
        MALLOC_STAT_WRITE_LOG(buf, s);
        }
        */

        s = snprintf(buf, sizeof(buf), "+ INIT \n-\n");
        log_mem("INIT", &static_buffer, static_pointer);
        // MALLOC_STAT_WRITE_LOG(buf, s);
    }

    return 0;
}

void malloc_stat_fini_lib(void) {
    /* check already finalized */
    if ( !__sync_bool_compare_and_swap(&init_done,
        LOG_MALLOC_INIT_DONE, LOG_MALLOC_FINI_DONE) )
    {
        return;
    }

    if ( !memlog_disabled ) {
        int s;
        char buf[LOG_BUFSIZE];

        s = snprintf(
             buf, sizeof(buf)
            ,"+=============================================================================\n"
             "| allocs  : %-10" PRIu64 ", deallocs: %-10" PRIu64 ", inuse: %-10" PRIu64 "\n"
             "| AL bytes: %-10" PRIu64 ", DE bytes: %-10" PRIu64 ", peak : %-10" PRIu64 "\n"
             "+=============================================================================\n"
            ,total_allocations, total_deallocations, total_in_use
            ,total_allocated, total_deallocated, peak_in_use
        );

        s += snprintf(buf + s, sizeof(buf) - s, "+ FINI\n-\n");
        MALLOC_STAT_WRITE_LOG(buf, s);
    }

    return;
}

static void __attribute__((constructor))
malloc_stat_init(void) {
    malloc_stat_init_lib();
    return;
}

static void __attribute__((destructor))
malloc_stat_fini(void) {
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

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_malloc(size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("malloc", ret, allocated);

    return ret;
}

void* calloc(size_t nmemb, size_t size) {
    if ( !DL_RESOLVE_CHECK(calloc) ) {
        return calloc_static(nmemb, size);
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_calloc(nmemb, size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("calloc", ret, allocated);

    return ret;
}

void* realloc(void *ptr, size_t size) {
    if ( !DL_RESOLVE_CHECK(realloc) ) {
        return NULL;
    }

    if ( ptr ) {
        size_t old_size = malloc_usable_size(ptr);
        MALLOC_STAT_SUB_IN_USE(old_size);

        if ( size ) { // realloc case
            void *ret = real_realloc(ptr, size);
            size_t new_size = malloc_usable_size(ret);

            MALLOC_STAT_ADD_IN_USE(new_size);

            MALLOC_STAT_INC_DEALLOCATIONS();
            MALLOC_STAT_INC_ALLOCATIONS();

            MALLOC_STAT_ADD_DEALLOCATED(old_size);
            MALLOC_STAT_ADD_ALLOCATED(new_size);

            MALLOC_STAT_TRACE((ptr != ret ? "realloc-realloc" : "realloc-inplace"), ret, new_size);

            return ret;
        } else { // free case
            MALLOC_STAT_INC_DEALLOCATIONS();

            MALLOC_STAT_ADD_DEALLOCATED(old_size);

            MALLOC_STAT_TRACE("realloc-free", ptr, old_size);

            return real_realloc(ptr, 0);
        }
    }

    if ( size ) { // malloc case
        void *ret = real_realloc(NULL, size);
        size_t allocated = malloc_usable_size(ret);
        
        MALLOC_STAT_INC_ALLOCATIONS();
        MALLOC_STAT_ADD_IN_USE(allocated);

        MALLOC_STAT_ADD_ALLOCATED(allocated);

        MALLOC_STAT_TRACE("realloc-malloc", ret, allocated);

        return ret;
    }

    return NULL;
}

void* memalign(size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(memalign) ) {
        return NULL;
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_memalign(alignment, size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("memalign", ret, allocated);

    return ret;
}

int posix_memalign(void **ptr, size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(posix_memalign) ) {
        return ENOMEM;
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    int ret = real_posix_memalign(ptr, alignment, size);
    size_t allocated = malloc_usable_size(*ptr);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("posix_memalign", *ptr, allocated);

    return ret;
}

void* valloc(size_t size) {
    if ( !DL_RESOLVE_CHECK(valloc) ) {
       return NULL;
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_valloc(size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("valloc", ret, allocated);

    return ret;
}

void* pvalloc(size_t size) {
    if( !DL_RESOLVE_CHECK(pvalloc) ) {
        return NULL;
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_pvalloc(size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("pvalloc", ret, allocated);

    return ret;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if ( !DL_RESOLVE_CHECK(aligned_alloc) ) {
        return NULL;
    }

    MALLOC_STAT_INC_ALLOCATIONS();

    void *ret = real_aligned_alloc(alignment, size);
    size_t allocated = malloc_usable_size(ret);

    MALLOC_STAT_ADD_ALLOCATED(allocated);
    MALLOC_STAT_ADD_IN_USE(allocated);

    MALLOC_STAT_TRACE("aligned_alloc", ret, allocated);

    return ret;
}

void free(void *ptr) {
    if ( !DL_RESOLVE_CHECK(free) ) {
        // We can not log anything here because the log message would result another free call and it would fall into an endless loop
        return;
    }

    MALLOC_STAT_INC_DEALLOCATIONS();

    if ( ptr ) {
        size_t allocated = malloc_usable_size(ptr);

        MALLOC_STAT_ADD_DEALLOCATED(allocated);
        MALLOC_STAT_SUB_IN_USE(allocated);

        MALLOC_STAT_TRACE("free", ptr, allocated);

        real_free(ptr);

        return;
    }

    MALLOC_STAT_TRACE("free(NULL)", NULL, 0);
}

/* EOF */
