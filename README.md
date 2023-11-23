malloc-stat
=================

**malloc-stat** is a completely reworked version of the [log-malloc-simple](https://github.com/qgears/log-malloc-simple) project which is a **pre-loadable** **C** library tracking all memory allocations//de-allocations of a program.

It produces simple text output by user request, that makes it easy to analyse alloc/free usage and find leaks and also identify their origin.

## Features

- counting number of memory allocation calls
- counting number of memory DEallocation calls
- counting amount of TOTAL allocated memory
- counting amount of TOTAL DEallocated memory
- counting amount of TOTAL simultaneously used memory
- counting PEAK amount of TOTAL simultaneously used memory
- logging to file descriptor 1022 (if opened)
- call stack **backtrace** using GNU [backtrace()](https://man7.org/linux/man-pages/man3/backtrace.3.html) (WIP)
- thread safe
- simple api to reset/get statistic on fly

## API

```c
#include <malloc-stat/api.h>

int main() {
    malloc_stat_get_stat_fnptr get_stat = MALLOC_STAT_GET_STAT_FNPTR();
    assert(get_stat);

    /* print some string to warming up the output stream so it can allocate required buffers */
    printf("hello from main!\n");

    /* grab the stat */
    malloc_stat_vars stat = MALLOC_STAT_GET_STAT(get_stat);

    /* print the stat */
    MALLOC_STAT_PRINT("before malloc", stat);

    volatile char *p = malloc(32);
    *p = 0;

    /* the easier way */
    MALLOC_STAT_SHOW("after malloc", get_stat);

    /* reset the collected stat */
    MALLOC_STAT_RESET_STAT(get_stat);

    /* enable logging */
    MALLOC_STAT_ENABLE_LOG();

    /* set the output FD as stdout */
    MALLOC_STAT_SET_LOG_FD(STDOUT_FILENO);

    p = realloc((void *)p, 64);

    MALLOC_STAT_SHOW("after realloc", get_stat);

    /* `p` was not freed, so we will see that in the report produced 
     * into `stdout` the leaked memory on destruction stage of malloc-stat.so
     */
    return *p;
}
```

## Caveats

- When using glib, use `G_SLICE=always-malloc` environment variable value so that g_slice allocations are better trackable (in case of a leak there will be no false blame of a different component).
- In some cases pthread_create call has a phantom free that frees memory block that was never allocated. I guess it can somehow call original malloc without going through the anchor functions.

## Dependencies

- `malloc_usable_size()` method - could be get rid of but we could not track the size of freed memory chunks. In case we analyze all the logs of the whole lifecycle of the program then it could be accepted.
- `/proc/self/exe`, `/proc/self/cwd`

## Usage

- `LD_PRELOAD=./malloc-stat.so command args ... 1022>/tmp/program.log`

or it is possible to send the logs to a pipe that is processed on the same computer:

- `LD_PRELOAD=./malloc-stat.so command args ... 1022>/tmp/malloc.pipe`

or it is possible to send the logs to TCP directly:

- `LD_PRELOAD=./malloc-simple.so command args ... 1022>/dev/tcp/$HOSTNAME/$PORT`

On the log processing computer a pipe and netcat can be used to direct the data into the log analyser tool.

## Building instructions

- `cd src && make`
- `cd src && make run-test`

## Log file format

```
# PID 5625
# EXE /usr/bin/gedit
# CWD /home/nixman/projects/malloc-stat/src
# MAPS

...

+ INIT 32 0x7f1a9b04e140
-
+ malloc 40 0xadf010
-

...

+ free 24 0x1db9360
-
```

* Log stream begins with basic process information lines beginning with `#`
* `#MAPS` ...: content of /proc/self/maps
* Log entries start with a line beginning with `+` followed by an entry type name and two numbers (all separated by single space characters):
    * First is size in bytes as a decimal number
    * Second is memory address as a hexadecimal constant (eg. 0x1db9010)
* End with a line beginning with `-`
* Log entry types are:
    * `INIT` - (size and address parameter is not important) means that the analyser tool is set up
    * `FINI` - (no size and address parameter) means that the process quit
    * `malloc`, `calloc`, `memalign`, `posix_memalign`, `valloc`, `pvalloc`, `aligned_alloc`, `free`: These methods have the same names in C
    * realloc calls result in double-staged log entries:
        - `realloc-alloc`: a memory was really allocated
        - `realloc-inplace`: a memory was expanded on the same area
        - `realloc-realloc`: a memory was really freed and allocated
        - `realloc-free`: a memory was freed
* Log entries are closed with a line starting with `-` character

# Author

- ***niXman***
- **contact**: https://github.com/niXman
* Based on previous works by Samuel Behan and Ivan Tikhonov and András Schmidt - see comments in malloc-stat.c

# Licensing

* [LGPLv3](https://www.gnu.org/licenses/lgpl.html) for C code (library)

