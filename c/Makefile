SHELL := /bin/bash

.PHONY: run-hellow

all: malloc-stat.so

malloc-stat.so: malloc-stat.c
	gcc -Wno-unused-result -o malloc-stat.so -shared -fPIC -DPIC malloc-stat.c -ldl -nostartfiles -O2

hellow: hellow.c
	gcc -o hellow -rdynamic hellow.c

# Example that must be executed with a java analyzer already existing
run-hellow-tcp: hellow malloc-stat.so
	LD_PRELOAD=./malloc-stat.so ./hellow 1022>/dev/tcp/localhost/9999

# Example that puts all output to stdout just to show how it looks
run-hellow: hellow malloc-stat.so
	LD_PRELOAD=./malloc-stat.so ./hellow 1022>&1

clean:
	rm -f malloc-stat.so
