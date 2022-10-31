all: build run

build:
	gcc-12 -std=gnu99 -o main main.c

run:
	./main