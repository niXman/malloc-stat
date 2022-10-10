/*
 * This file is part of malloc-stat project.
 * Alloca/free library with backtrace for not freed areas and byte-exact memory tracking.
 * Author: niXman, 2022 year
 * https://github.com/niXman/malloc-stat
 */

#ifndef __malloc_stat__api_h
#define __malloc_stat__api_h

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <malloc.h>
#include <dlfcn.h>

/* will returns the size of really allocated block.
 * just a wraper for the malloc's malloc_usable_size()
 */
#define MALLOC_STAT_ALLOCATED_SIZE(ptr) \
    malloc_usable_size(ptr)

/* the malloc_stat_vars struct is used for retrieving
 * the information about the current stat.
 */
typedef struct malloc_stat_vars {
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t in_use;
} malloc_stat_vars;

/* calculate the difference */
#define MALLOC_STAT_GET_DIFF(before, after) \
    (malloc_stat_vars){ \
         after.allocations - before.allocations \
        ,after.deallocations - before.deallocations \
        ,after.in_use - before.in_use}

/* test for equality */
#define MALLOC_STAT_IS_EQUAL(left, right) \
    (left.allocations == right.allocations \
        && left.deallocations == right.deallocations \
        && left.in_use == right.in_use)

/* the signature of the provided function pointer used to obtain a stat */
typedef malloc_stat_vars (*malloc_stat_get_stat_fnptr)(void);

/* provided to an user to obtain an address of the function
 * which can be used to obtain a stat information.
 * example:
 *
 * int main() {
 *     malloc_stat_get_stat_fnptr get_stat = MALLOC_STAT_GET_STAT_FNPTR();
 *     assert(get_stat);
 *     malloc_stat_vars stat = get_stat();
 * }
 */
#define MALLOC_STAT_GET_STAT_FNPTR() \
    (malloc_stat_get_stat_fnptr)dlsym(RTLD_DEFAULT, "malloc_stat_get_stat")

/* just a helpers.
 * example:
 *
 * int main() {
 *     malloc_stat_get_stat_fnptr get_stat = MALLOC_STAT_GET_STAT_FNPTR();
 *     assert(get_stat);
 *     MALLOC_STAT_FPRINT(stdout, "before any allocation", get_stat);
 * }
 */
#define MALLOC_STAT_FPRINT(stream, caption, fnptr) { \
    malloc_stat_vars stat; \
    if ( fnptr ) { \
        stat = fnptr(); \
    } \
    fprintf(stream \
        ,"%s: +++ %" PRIu64 ", --- %" PRIu64 ", === %" PRIu64 "\n" \
        ,caption \
        ,stat.allocations \
        ,stat.deallocations \
        ,stat.in_use \
    ); \
}

#define MALLOC_STAT_PRINT(caption, fnptr) \
    MALLOC_STAT_FPRINT(stdout, caption, fnptr)

#endif // __malloc_stat__api_h
