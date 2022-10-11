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

static const char* test_00() {
    malloc_stat_vars before, after, diff;

    before = MALLOC_STAT_RESET_STAT(get_stat);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after = MALLOC_STAT_GET_STAT(get_stat);
    diff = MALLOC_STAT_GET_DIFF(before, after);

    if ( diff.allocations != 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( diff.deallocations != 0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    uint64_t allocated = MALLOC_STAT_ALLOCATED_SIZE(p);
    if ( diff.in_use != allocated ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( diff.peak_in_use != allocated) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    return NULL;
}

static const char* test_01() {
    malloc_stat_vars before_malloc, after_malloc
        ,after_free, diff_after_malloc;

    before_malloc = MALLOC_STAT_RESET_STAT(get_stat);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    after_malloc = MALLOC_STAT_GET_STAT(get_stat);
    diff_after_malloc = MALLOC_STAT_GET_DIFF(before_malloc, after_malloc);

    if ( diff_after_malloc.allocations != 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( diff_after_malloc.deallocations != 0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    uint64_t allocated = MALLOC_STAT_ALLOCATED_SIZE(p);
    if ( diff_after_malloc.in_use != allocated ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( diff_after_malloc.peak_in_use != allocated ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    free(p);

    after_free = MALLOC_STAT_GET_STAT(get_stat);
    if ( after_free.allocations != before_malloc.allocations + 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( after_free.deallocations != before_malloc.deallocations + 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( after_free.in_use != before_malloc.in_use ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( diff_after_malloc.peak_in_use != allocated ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    return NULL;
}

/*************************************************************************************************/

// realloc test
static const char* test_02() {
    malloc_stat_vars stat;

    MALLOC_STAT_RESET_STAT(get_stat);

    // realloc alloc case
    void *p = realloc(NULL, 32);
    uint64_t allocated_0 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.in_use != allocated_0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.allocated != allocated_0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.peak_in_use != allocated_0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    // realloc inplace
    p = realloc(p, 64);
    uint64_t allocated_1 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 2) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.in_use != allocated_1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.allocated != allocated_0 + allocated_1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocations != 1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocated != allocated_0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.peak_in_use != allocated_1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    // real realloc
    p = realloc(p, 1024*1024);
    uint64_t allocated_2 = MALLOC_STAT_ALLOCATED_SIZE(p);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 3 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocations != 2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.allocated != allocated_0 + allocated_1 + allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocated != allocated_0 + allocated_1 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.in_use != allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.peak_in_use != allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    // realloc free case
    p = realloc(p, 0);

    stat = MALLOC_STAT_GET_STAT(get_stat);
    if ( stat.allocations != 3 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocations != 3 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.allocated != allocated_0 + allocated_1 + allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.deallocated != allocated_0 + allocated_1 + allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.in_use != 0 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }
    if ( stat.peak_in_use != allocated_2 ) {
        return MALLOC_STAT_MAKE_FILE_LINE();
    }

    return NULL;
}

/*************************************************************************************************/

#define TEST(name) { \
    const char *r = name(); \
    fprintf(stdout, "test \"%s\" - %5s, ec=%s\n", #name, (!r ? "OK" : "ERROR"), r); \
}

int main() {
    /* warming up the output stream so it can allocate required buffers */
    fprintf(stdout, "warming up the stdout stream!\n");

    get_stat = MALLOC_STAT_GET_STAT_FNPTR();
    assert(get_stat);

    MALLOC_STAT_PRINT("stat from main()", get_stat);

    TEST(test_00);
    TEST(test_01);
    TEST(test_02);
}

/*************************************************************************************************/
