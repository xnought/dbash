CC=gcc
CFLAGS=-std=gnu99
NAME=dbash

all: build 

build:
	$(CC) $(CFLAGS) -o $(NAME) $(NAME).c

run:
	./$(NAME)