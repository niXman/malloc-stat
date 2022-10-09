/*
 * This file is the part of malloc-stat project.
 * Alloca/free library with backtrace for not freed areas and byte-exact memory tracking.
 * Author: niXman, 2022 year
 * https://github.com/niXman/malloc-stat
 *
 * the status can be obtained by using:
 * void malloc_stat_get_stat(uint64_t *allocations, uint64_t *deallocations, uint64_t *in_use);
 *
 * Based on previous implementations by other authors. See their comments below:
 */

#include <malloc-stat/api.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

malloc_stat_get_stat_fnptr malloc_stat_get_stat = NULL;

void malloc_stat_fnptr_received(malloc_stat_get_stat_fnptr fnptr) {
    malloc_stat_get_stat = fnptr;
}

/*************************************************************************************************/

int test_00() {
    malloc_stat_vars before, after, diff;

    malloc_stat_get_stat(&before);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    malloc_stat_get_stat(&after);
    diff = malloc_stat_get_diff(&before, &after);

    if ( diff.allocations != 1 ) {
        return 1;
    }
    if ( diff.deallocations != 0 ) {
        return 2;
    }
    if ( diff.in_use != malloc_stat_allocated_size(p) ) {
        return 3;
    }

    return 0;
}

int test_01() {
    malloc_stat_vars before_malloc, after_malloc
        ,after_free, diff_after_malloc;

    malloc_stat_get_stat(&before_malloc);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    malloc_stat_get_stat(&after_malloc);
    diff_after_malloc = malloc_stat_get_diff(&before_malloc, &after_malloc);

    if ( diff_after_malloc.allocations != 1 ) {
        return 1;
    }
    if ( diff_after_malloc.deallocations != 0 ) {
        return 2;
    }
    uint64_t allocated_size = malloc_stat_allocated_size(p);
    if ( diff_after_malloc.in_use != allocated_size ) {
        return 3;
    }

    free(p);

    malloc_stat_get_stat(&after_free);
    if ( after_free.allocations != before_malloc.allocations + 1 ) {
        return 4;
    }
    if ( after_free.deallocations != before_malloc.deallocations + 1 ) {
        return 5;
    }
    if ( after_free.in_use != before_malloc.in_use ) {
        return 5;
    }

    return 0;
}

/*************************************************************************************************/

#define TEST(name) { \
    int r = name(); \
    fprintf(stdout, "test \"%s\" - %5s, ec=%d\n", #name, (!r ? "OK" : "ERROR"), r); \
}

int main() {
    /* warming up the output stream so it can allocate required buffers */
    fprintf(stdout, "warming up the stdout stream!\n");
    MALLOC_STAT_PRINT("hello from test!", malloc_stat_get_stat);

    TEST(test_00);
    TEST(test_01);
}

/*************************************************************************************************/
