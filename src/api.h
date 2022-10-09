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

/* will returns the size of really allocated block.
 * just a wraper for the malloc's malloc_usable_size()
 */
uint64_t malloc_stat_allocated_size(void *ptr);

typedef void (*malloc_stat_get_stat_fnptr)(uint64_t *allocations, uint64_t *deallocations, uint64_t *in_use);

/* the user implemented handler that will be called by malloc-stat.so
 * on its initialization stage to provide to user an address of the function
 * which hi/she can use in his/her code to obtain stat information.
 * example:
 * malloc_stat_get_stat_fnptr get_stat = NULL;
 *
 * void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr) {
 *     get_stat = fnptr;
 * }
 *
 * int main() {
 *     uint64_t allocations = 0, deallocations = 0, inuse = 0;
 *     get_stat(&allocations, &deallocations, &inuse);
 * }
 */
extern void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr);

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
    uint64_t allocations, deallocations, in_use; \
    if ( fnptr ) { \
        fnptr(&allocations, &deallocations, &in_use); \
    } \
    fprintf(stream \
        ,"%s: +++ %" PRIu64 ", --- %" PRIu64 ", === %" PRIu64 "\n" \
        ,caption \
        ,allocations \
        ,deallocations \
        ,in_use \
    ); \
}

#define MALLOC_STAT_PRINT(caption, fnptr) \
    MALLOC_STAT_FPRINT(stdout, caption, fnptr)

#endif // __malloc_stat__api_h
