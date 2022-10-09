
SHELL  := /bin/bash
CFLAGS := -I$$PWD/../include -Wall -Wextra -Werror -Wno-unused-result -O2
LDFLAGS:= -fPIC -ldl

.PHONY: all

all: malloc-stat.so hellow test

malloc-stat.so: malloc-stat.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -nostartfiles malloc-stat.c -o malloc-stat.so

test: test.c
	$(CC) $(CFLAGS) $(LDFLAGS) -rdynamic test.c api.c -o test

hellow: hellow.c
	$(CC) $(CFLAGS) $(LDFLAGS) -rdynamic hellow.c -o hellow

run-test: test malloc-stat.so
	LD_PRELOAD=./malloc-stat.so ./test

# Example that must be executed with a java analyzer already existing
run-hellow-tcp: hellow malloc-stat.so
	LD_PRELOAD=./malloc-stat.so ./hellow 1022>/dev/tcp/localhost/9999

# Example that puts all output to stdout just to show how it looks
run-hellow: hellow malloc-stat.so
	LD_PRELOAD=./malloc-stat.so ./hellow 1022>&1

clean:
	rm -f malloc-stat.so hellow test
