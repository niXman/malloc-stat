TODO! - to rewrite!
=================

malloc-stat
=================

*malloc-stat* is **pre-loadable** library tracking all memory allocations of a program. It produces simple text trace output by user request, that makes it easy to find leaks and also identify their origin.

The output of *malloc-stat* can be easily parsed by a log analyzer tool.

## Changes compared to log-malloc-simple

The simplifications and their reason:

- The return value of all allocations is returned intact. This prevents some crashes that were caused by log-malloc2 due to lack of memory alignment returned by modified calloc method.
- Only malloc_usable_size() is used to track the size of allocated memory.
- Conditional compilation is removed. I have no chance to test all versions of the code. Only platforms with GNU backtrace and malloc_usable_size are supported.
- Data aggregation of the logger is removed. It is delegated to a separate log-analyzer tool.

## Features

- logging to file descriptor 1022 (if opened)
- call stack **backtrace** (using GNU backtrace())
- thread safe
- fork is detected (PID is part of each log message) and allocations in the forked process are ignored (they caused noise in the logs)

## Caveats

- When using glib, use `G_SLICE=always-malloc` environment variable value so that g_slice allocations are better trackable (in case of a leak there will be no false blame of a different component).
- In some cases pthread_create call has a phantom free that frees memory block that was never allocated. I guess it can somehow call original malloc without going through the anchor functions.

## Dependencies

- malloc_usable_size() method - could be get rid of but we could not track the size of freed memory chunks. In case we analyze all the logs of the whole lifecycle of the program then it could be accepted.
- /proc/self/exe, /proc/self/cwd

## Usage

`LD_PRELOAD=./log-malloc-simple.so command args ... 1022>/tmp/program.log`

or it is possible to send the logs to a pipe that is processed on the same computer:

`LD_PRELOAD=./log-malloc-simple.so command args ... 1022>/tmp/malloc.pipe`

or it is possible to send the logs to TCP directly:

`LD_PRELOAD=./log-malloc-simple.so command args ... 1022>/dev/tcp/$HOSTNAME/$PORT`

On the log processing computer a pipe and netcat can be used to direct the data into the log analyser tool.

### Log analyzer tool

Standalone program written in Java. Usage:

- Create a pipe that will transfer memory allocation log to the Java program: `$ mkfifo /tmp/malloc.pipe`
- Start analyzer: `$ java -jar analyzer.jar /tmp/malloc.pipe`
- Use console to command analyzer: stop/start collecting data, print or save current state to file
- Start program to analyze: `$ LD_PRELOAD=./log-malloc-simple.so my_executable args ... 1022>/tmp/malloc.pipe`
- Optional: Create a snapshot of all active memory allocations by command `snapshot 00.snapshot`
- Run test cases that should run without leaking memory.
- Optional: Create a snapshot of all active memory allocations by command `snapshot 01.snapshot`
- Optional: Execute the compare snapshots mode of the analyser to see comparison of the allocated blocks. (See below)
- See the output of the analyzer for non-freed memory chunks.
- Log output may also be directed to a static file and analysed offline.
- Input of analyser may be redirected. This way it can be used in an automated process.

## Building instructions

### The maven way

```
git clone https://github.com/qgears/opensource-utils.git
git clone https://github.com/qgears/log-malloc-simple.git
cd log-malloc-simple
mvn package
```
The resulting, standalone jar, with all its dependencies packed in, will be located as follows:
`java/hu.qgears.analyzelogmalloc/target/analyzer.jar`

### Eclipse IDE
- The analyser is written in Java and stored as an Eclipse project. (The project uses OSGI for dependency management but must be run as a standalone Java program.)
- import the `hu.qgears.analyzelogmalloc` and `hu.qgears.commons` projects into an Eclipse workspace
 - hint: `hu.qgears.commons` dependency is located in the  https://github.com/qgears/opensource-utils repo
- create a Java app. run configuration for the `Analyze` class. Analyser may be used by launching inside Eclipse as a standalone applocation.
- export the run configuration as an executable jar
- TODO: implement MAVEN build

## Log file format

```
# PID 5625
# EXE /usr/bin/gedit
# CWD /home/rizsi/github-qgears/log-malloc-simple/c
# MAPS

...

+ INIT 32 0x7f1a9b04e140
-
+ malloc 40 0xadf010
/usr/lib/x86_64-linux-gnu/libstdc++.so.6(_Znwm+0x18)[0x7f1a9156e698]
/usr/lib/x86_64-linux-gnu/libstdc++.so.6(_ZNSs4_Rep9_S_createEmmRKSaIcE+0x59)[0x7f1a915d26f9]
/usr/lib/x86_64-linux-gnu/libstdc++.so.6(_ZNSs12_S_constructIPKcEEPcT_S3_RKSaIcESt20forward_iterator_tag+0x35)[0x7f1a915d4165]
/usr/lib/x86_64-linux-gnu/libstdc++.so.6(_ZNSsC2EPKcRKSaIcE+0x36)[0x7f1a915d45b6]
/usr/lib/x86_64-linux-gnu/libboost_filesystem.so.1.55.0(+0x6921)[0x7f1a8ee3a921]
/lib64/ld-linux-x86-64.so.2(+0x105ba)[0x7f1a9b05f5ba]
-

...

+ free 24 0x1db9360
-
```

* Log stream begins with basic process information lines beginning with #
* #MAPS ...: content of /proc/self/maps
* Log entries start with a line beginning with '+' followed by an entry type name and two numbers (all separated by single space characters):
    * First is size in bytes as a decimal number
    * Second is memory address as a hexadecimal constant (eg. 0x1db9010)
end with a line beginning with '-'
* Log entry types are:
    * "INIT" - (size and address parameter is not important) means that the analyser tool is set up
    * "FINI" - (no size and address parameter) means that the process quit
    * "malloc", "calloc", "memalign", "posix_memalign", "valloc", "free": These methods have the same names in C
    * "realloc_free", "realloc_alloc": realloc calls result in two separate log entries one for free and one for allocation
* Allocation log entries contain stack trace where the allocation related method was called from
* Log entries are closed with a line starting with "-" character

## Analysed data output

Analyser waits for commands on stdin:

 * off - turn analyzer off while the application is setting up (to spare CPU cycles when analysation is not required - eg initialization of the program)
 * on - turn analyzer on when the critical session is started (default is on)
 * (reset - clear all log entries cached by the analyzer)
 * print - print current allocation status (since last reset/on) to stdout
 * save <filename> - print current allocation status (since last reset/on) to file
 * snapshot <filename> - save all current stored allocations into a file (see compare below)

The analysed data output is in text format. After a short summary all not-freed allocations are listed. These entries are ordered and summarised by the identifier of the instruction (library+pointer) calling the allocation method. The textual output of the same program in different moments may be compared to each other using text comparing tools to find leaks. (A single call from each allocator calling instruction is printed as an example in the output but this does not mean that it is the only possible stack trace that calls this leaking method.):

```
Processing timespan in millis (since first log processed after reset, measured with currentTimeMillis): 9,331
Allocation balance (bytes, negative means leak): -353,560
Number of objects allocated in log session but not freed yet: 2714
Size of objects freed in log session but not allocated in log session (bytes): 2,888
Number of objects freed in session but not allocated in session: 59 without multiple frees: 50
Matching alloc/free pairs through the logging session (n, bytes): 4144 383,696

allocator: /usr/lib/x86_64-linux-gnu/libpixman-1.so.0(+0x58c6b)[0x7ff6ccd50c6b]
	N:6 BYTES: 12,432
+ malloc 2072 0x1fb0f40

	/usr/lib/x86_64-linux-gnu/libpixman-1.so.0(+0x58c6b)[0x7ff6ccd50c6b]
	/usr/lib/x86_64-linux-gnu/libpixman-1.so.0(+0x93b90)[0x7ff6ccd8bb90]
	/usr/lib/x86_64-linux-gnu/libpixman-1.so.0(+0x5934c)[0x7ff6ccd5134c]
	/usr/lib/x86_64-linux-gnu/libpixman-1.so.0(+0x9a09)[0x7ff6ccd01a09]
	/lib64/ld-linux-x86-64.so.2(+0x105ba)[0x7ff6d370c5ba]
	/lib64/ld-linux-x86-64.so.2(+0x106cb)[0x7ff6d370c6cb]

...

```

## Compare snapshots

Command line: ```--compare <fileState2> --pipe <fileState1>``` --printAllIfContains pattern

What it does:

 * process both fileState1 and fileState1 normally into a separate model in memory
 * diff the the models in memory:
 * Number of objects, allocated bytes of objects by allocator identifier (the program line that calls to allocator) is counted for each allocator
 ** First same allocations are removed
 ** Freed and new allocations are diffed by size and number
 ** new allocations are part of the report
 * Output is ordered by the size of the increment (possible leak) by allocator identifier
 * --printAllIfContains (multiple instances are possible) if a pattern is present in the stack trace of the first example then all examples are written into the output. Useful in case we find a leak that starts like the one below and we want to see each distinct stack traces which end in this leaking function call. Then we add the ```--printAllIfContains g_malloc+0x29``` parameter:

``` 
/lib/i386-linux-gnu/libglib-2.0.so.0(g_malloc+0x29) [0x93737a39]
/lib/i386-linux-gnu/libglib-2.0.so.0(g_slice_alloc+0x49) [0x937501a9]
```

# Author

- ***András Schmidt***
 - **contact**: https://github.com/rizsi (author's Github page)
* Based on previous works by Samuel Behan and Ivan Tikhonov - see comments in log-malloc-simple.c

# Licensing

* [LGPLv3](https://www.gnu.org/licenses/lgpl.html) for C code (library)
* [GPLv3](https://www.gnu.org/licenses/gpl.html) for Java malloc log analyzer

