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

#ifdef __cplusplus
#   define MALLOC_STAT_LANG_LINKAGE extern "C"
#else
#   define MALLOC_STAT_LANG_LINKAGE
#endif // __cplusplus

/* will returns the size of really allocated block.
 * just a wraper for the malloc's malloc_usable_size()
 */
MALLOC_STAT_LANG_LINKAGE
size_t malloc_stat_allocated_size(void *ptr);

/* the malloc_stat_vars struct is used for retrieving and to calc
 * a difference betwen "before" and "after" test cases.
 */
typedef struct malloc_stat_vars {
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t in_use;
} malloc_stat_vars;

/* calculate the difference */
MALLOC_STAT_LANG_LINKAGE
malloc_stat_vars malloc_stat_get_diff(
     const malloc_stat_vars *before
    ,const malloc_stat_vars *after
);

/* test for equality */
MALLOC_STAT_LANG_LINKAGE
int malloc_stat_is_equal(
     const malloc_stat_vars *l
    ,const malloc_stat_vars *r
);

/* the signature of the provided function pointer used to obtain a stat */
typedef void (*malloc_stat_get_stat_fnptr)(malloc_stat_vars *ptr);

/* the user implemented handler that will be called by malloc-stat.so
 * on its initialization stage to provide to user an address of the function
 * which can be used to obtain a stat information.
 * example:
 * malloc_stat_get_stat_fnptr get_stat = NULL;
 *
 * void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr) {
 *     get_stat = fnptr;
 * }
 *
 * int main() {
 *     malloc_stat_vars stat;
 *     get_stat(&stat);
 * }
 */
MALLOC_STAT_LANG_LINKAGE
void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr);

/* just a helpers.
 * example:
 * malloc_stat_get_stat_fnptr get_stat = NULL;
 *
 * void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr) {
 *     get_stat = fnptr;
 * }
 *
 * int main() {
 *     MALLOC_STAT_FPRINT(stdout, "before any allocation", get_stat);
 * }
 */
#define MALLOC_STAT_FPRINT(stream, caption, fnptr) { \
    malloc_stat_vars stat; \
    if ( fnptr ) { \
        fnptr(&stat); \
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
