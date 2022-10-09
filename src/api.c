
#include <malloc.h>

#include "../include/malloc-stat/api.h"

size_t malloc_stat_allocated_size(void *ptr) {
    return malloc_usable_size(ptr);
}

malloc_stat_vars malloc_stat_get_diff(
     const malloc_stat_vars *before
    ,const malloc_stat_vars *after
) {
    malloc_stat_vars res = {
         after->allocations - before->allocations
        ,after->deallocations - before->deallocations
        ,after->in_use - before->in_use
    };

    return res;
}

int malloc_stat_is_equal(
     const malloc_stat_vars *l
    ,const malloc_stat_vars *r
) {
    return l->allocations == r->allocations
        && l->deallocations == r->deallocations
        && l->in_use == r->in_use
    ;
}
