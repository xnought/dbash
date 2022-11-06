all: build 

build:
	gcc-12 -std=gnu99 -o smallsh smallsh2.c

run:
	./smallsh

test:
	./p3testscript > mytestresults 2>&1 