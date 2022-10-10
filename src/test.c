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
#include <assert.h>

malloc_stat_get_stat_fnptr get_stat = NULL;

/*************************************************************************************************/

int test_00() {
    malloc_stat_vars before, after, diff;

    before = get_stat();
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after = get_stat();
    diff = MALLOC_STAT_GET_DIFF(before, after);

    if ( diff.allocations != 1 ) {
        return 1;
    }
    if ( diff.deallocations != 0 ) {
        return 2;
    }
    if ( diff.in_use != MALLOC_STAT_ALLOCATED_SIZE(p) ) {
        return 3;
    }

    return 0;
}

int test_01() {
    malloc_stat_vars before_malloc, after_malloc
        ,after_free, diff_after_malloc;

    before_malloc = get_stat();
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after_malloc = get_stat();
    diff_after_malloc = MALLOC_STAT_GET_DIFF(before_malloc, after_malloc);

    if ( diff_after_malloc.allocations != 1 ) {
        return 1;
    }
    if ( diff_after_malloc.deallocations != 0 ) {
        return 2;
    }
    uint64_t allocated_size = MALLOC_STAT_ALLOCATED_SIZE(p);
    if ( diff_after_malloc.in_use != allocated_size ) {
        return 3;
    }

    free(p);

    after_free = get_stat();
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

    get_stat = MALLOC_STAT_GET_STAT_FNPTR();
    assert(get_stat);

    MALLOC_STAT_PRINT("hello from test!", get_stat);

    TEST(test_00);
    TEST(test_01);
}

/*************************************************************************************************/
