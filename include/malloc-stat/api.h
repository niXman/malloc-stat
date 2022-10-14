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

/* stringize macro
 */
#define MALLOC_STAT_STRINGIZE_I(x) #x
#define MALLOC_STAT_STRINGIZE(x) \
    MALLOC_STAT_STRINGIZE_I(x)

/*
 * MALLOC_STAT_VERSION / 100000 is the major version
 * MALLOC_STAT_VERSION / 100 % 1000 is the minor version
 * MALLOC_STAT_VERSION % 100 is the bugfix level
*/

#define MALLOC_STAT_VERSION_MAJOR 0
#define MALLOC_STAT_VERSION_MINOR 0
#define MALLOC_STAT_VERSION_BUGFIX 1

#define MALLOC_STAT_VERSION \
     MALLOC_STAT_VERSION_MAJOR*100000 \
    +MALLOC_STAT_VERSION_MINOR*1000 \
    +MALLOC_STAT_VERSION_BUGFIX*10

#define MALLOC_STAT_VERSION_STRING \
        MALLOC_STAT_STRINGIZE(MALLOC_STAT_VERSION_MAJOR) \
    "." MALLOC_STAT_STRINGIZE(MALLOC_STAT_VERSION_MINOR) \
    "." MALLOC_STAT_STRINGIZE(MALLOC_STAT_VERSION_BUGFIX)

/* compares the 'MALLOC_STAT_VERSION_MAJOR' with
 * the sealed in the malloc-stat.so library
 */

#define MALLOC_STAT_CHECK_VERSION() ({ \
    uint32_t (*fnptr)() = (uint32_t (*)()) \
        dlsym(RTLD_DEFAULT, "malloc_stat_get_version"); \
    uint32_t version = (fnptr ? fnptr() : 111111); \
    version /= 100000; \
    version == MALLOC_STAT_VERSION_MAJOR; \
})

/* file-line error string maker macro
 */
#define MALLOC_STAT_MAKE_FILE_LINE() \
    __FILE__ "(" MALLOC_STAT_STRINGIZE(__LINE__) ")"

/* will returns the size of really allocated block.
 * just a wraper for the malloc's malloc_usable_size()
 */
#define MALLOC_STAT_ALLOCATED_SIZE(ptr) \
    malloc_usable_size(ptr)

/* the malloc_stat_vars struct is used for represent
 * the information about the current stat.
 */
typedef struct {
    uint64_t allocations;
    uint64_t allocated;
    uint64_t deallocations;
    uint64_t deallocated;
    uint64_t in_use;
    uint64_t peak_in_use;
} malloc_stat_vars;

/* calculate the difference */
#define MALLOC_STAT_GET_DIFF(before, after) \
    (malloc_stat_vars){ \
         after.allocations   - before.allocations \
        ,after.allocated     - before.allocated \
        ,after.deallocations - before.deallocations \
        ,after.deallocated   - before.deallocated \
        ,after.in_use        - before.in_use \
        ,after.peak_in_use   - before.peak_in_use \
    }

/* test for equality */
#define MALLOC_STAT_IS_EQUAL(left, right) \
    (left.allocations == right.allocations \
        && left.deallocations == right.deallocations \
        && left.in_use == right.in_use)

/* the valid operations for 'malloc_stat_get_stat' function */
typedef enum {
     MALLOC_STAT_GET   /* get collented stat */
    ,MALLOC_STAT_RESET /* reset collented stat */
} malloc_stat_operation;

/* the signature of the provided function pointer used to obtain a stat */
typedef malloc_stat_vars (*malloc_stat_get_stat_fnptr)(malloc_stat_operation op);

/* just a helpers.
 */
#define MALLOC_STAT_GET_STAT(fnptr) \
    (fnptr ? fnptr(MALLOC_STAT_GET) : (malloc_stat_vars){})

#define MALLOC_STAT_RESET_STAT(fnptr) \
    (fnptr ? fnptr(MALLOC_STAT_RESET) : (malloc_stat_vars){})

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

/* turn on or turn off the logging outout printed to the specified fd-descriptor
 */
#define MALLOC_STAT_ENABLE_LOG() do { \
    void (*fnptr)(int) = dlsym(RTLD_DEFAULT, "malloc_stat_change_log_state"); \
    if ( fnptr ) fnptr(1); \
} while (0)

#define MALLOC_STAT_DISABLE_LOG() do { \
    void (*fnptr)(int) = dlsym(RTLD_DEFAULT, "malloc_stat_change_log_state"); \
    if ( fnptr ) fnptr(0); \
} while (0)

/* setting up the FD for logging output
 */
#define MALLOC_STAT_SET_LOG_FD(fd) do { \
    void (*fnptr)(int) = dlsym(RTLD_DEFAULT, "malloc_stat_set_log_fd"); \
    if ( fnptr ) fnptr(fd); \
} while (0)

/* just a helpers.
 * example:
 *
 * int main() {
 *     malloc_stat_get_stat_fnptr get_stat = MALLOC_STAT_GET_STAT_FNPTR();
 *     assert(get_stat);
 *     MALLOC_STAT_SHOW(stdout, "before any allocation", get_stat);
 * }
 */
#define MALLOC_STAT_FPRINT(stream, caption, stat) \
    fprintf(stream \
        ,"%s:\n" \
         "+==========================================================================+\n" \
         "| allocs  : %-14" PRIu64 "| deallocs: %-14" PRIu64 "| inuse: %-14" PRIu64 "|\n" \
         "| AL bytes: %-14" PRIu64 "| DE bytes: %-14" PRIu64 "| peak : %-14" PRIu64 "|\n" \
         "+==========================================================================+\n" \
        ,caption \
        ,stat.allocations \
        ,stat.deallocations \
        ,stat.in_use \
        ,stat.allocated \
        ,stat.deallocated \
        ,stat.peak_in_use \
    );

#define MALLOC_STAT_PRINT(caption, stat) \
    MALLOC_STAT_FPRINT(stdout, caption, stat)

#define MALLOC_STAT_FSHOW(stream, caption, fnptr) \
    MALLOC_STAT_FPRINT(stream, caption, MALLOC_STAT_GET_STAT(fnptr))

#define MALLOC_STAT_SHOW(caption, fnptr) \
    MALLOC_STAT_FPRINT(stdout, caption, MALLOC_STAT_GET_STAT(fnptr))

#endif // __malloc_stat__api_h
