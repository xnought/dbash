all: build 

build:
	gcc-12 -std=gnu99 -o small smallsh2.c

run:
	./small