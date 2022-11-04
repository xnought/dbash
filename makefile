all: build 

build:
	gcc-12 -std=gnu99 -o smallsh smallsh.c

run:
	./smallsh