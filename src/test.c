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
    uint64_t a = 0, d = 0, i = 0;
    malloc_stat_get_stat(&a, &d, &i);

    return !(a == 0 && d == 0 && i == 0);
}

int test_01() {
    uint64_t a0 = 0, d0 = 0, i0 = 0;
    uint64_t a1 = 0, d1 = 0, i1 = 0;

    malloc_stat_get_stat(&a0, &d0, &i0);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    malloc_stat_get_stat(&a1, &d1, &i1);
    if ( a1 != a0+1 ) {
        return 1;
    }
    if ( d1 != d0 ) {
        return 2;
    }
    if ( i1 != i0+malloc_stat_allocated_size(p) ) {
        return 3;
    }

    return 0;
}

int test_02() {
    uint64_t a0 = 0, d0 = 0, i0 = 0;
    uint64_t a1 = 0, d1 = 0, i1 = 0;
    uint64_t a2 = 0, d2 = 0, i2 = 0;

    malloc_stat_get_stat(&a0, &d0, &i0);
    void *p = malloc(32);
    memset(p, 'x', 32);
    (void)p;

    malloc_stat_get_stat(&a1, &d1, &i1);
    if ( a1 != a0+1 ) {
        return 1;
    }
    if ( d1 != d0 ) {
        return 2;
    }
    uint64_t allocated_size = malloc_stat_allocated_size(p);
    if ( i1 != i0 + allocated_size ) {
        return 3;
    }

    free(p);

    malloc_stat_get_stat(&a2, &d2, &i2);
    if ( a2 != a1 ) {
        return 4;
    }
    if ( d2 != d1+1 ) {
        return 5;
    }
    if ( i2 != i1 - allocated_size ) {
        return 6;
    }

    return 0;
}

/*************************************************************************************************/

#define TEST(name) { \
    int r = name(); \
    fprintf(stdout, "test \"%s\" was %5s, ec=%d\n", #name, (!r ? "OK" : "ERROR"), r); \
}

int main() {
    TEST(test_00);
    TEST(test_01);
    TEST(test_02);
}

/*************************************************************************************************/
