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

static int test_00() {
    malloc_stat_vars before, after, diff;

    before = MALLOC_STAT_RESET_STAT(get_stat);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after = MALLOC_STAT_GET_STAT(get_stat);
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

static int test_01() {
    malloc_stat_vars before_malloc, after_malloc
        ,after_free, diff_after_malloc;

    before_malloc = MALLOC_STAT_RESET_STAT(get_stat);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after_malloc = MALLOC_STAT_GET_STAT(get_stat);
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

    after_free = MALLOC_STAT_GET_STAT(get_stat);
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

// realloc test
static int test_02() {
    malloc_stat_vars stat;

    MALLOC_STAT_RESET_STAT(get_stat);

    // realloc alloc case
    void *p = realloc(NULL, 32);
    uint64_t allocated_0 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 1 ) {
        return 1;
    }
    if ( stat.in_use != allocated_0 ) {
        return 2;
    }
    if ( stat.allocated != allocated_0 ) {
        return 3;
    }

    // realloc inplace
    p = realloc(p, 64);
    uint64_t allocated_1 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 2) {
        return 4;
    }
    if ( stat.in_use != allocated_1 ) {
        return 5;
    }
    if ( stat.allocated != allocated_0 + allocated_1 ) {
        return 6;
    }
    if ( stat.deallocations != 1 ) {
        return 7;
    }
    if ( stat.deallocated != allocated_0 ) {
        return 8;
    }

    // real realloc
    p = realloc(p, 1024*1024);
    uint64_t allocated_2 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 3 ) {
        return 9;
    }
    if ( stat.deallocations != 2 ) {
        return 10;
    }
    if ( stat.allocated != allocated_0 + allocated_1 + allocated_2 ) {
        return 11;
    }
    if ( stat.deallocated != allocated_0 + allocated_1 ) {
        return 12;
    }
    if ( stat.in_use != allocated_2 ) {
        return 13;
    }

    // realloc free case
    p = realloc(p, 0);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 3 ) {
        return 14;
    }
    if ( stat.deallocations != 3 ) {
        return 15;
    }
    if ( stat.allocated != allocated_0 + allocated_1 + allocated_2 ) {
        return 16;
    }
    if ( stat.deallocated != allocated_0 + allocated_1 + allocated_2 ) {
        return 17;
    }
    if ( stat.in_use != 0 ) {
        return 18;
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
    TEST(test_02);
}

/*************************************************************************************************/
