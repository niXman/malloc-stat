
#include <malloc.h>

#include "api.h"

uint64_t malloc_stat_allocated_size(void *ptr) {
	return malloc_usable_size(ptr);
}
