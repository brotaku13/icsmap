CC = gcc
DFLAGS := -g
CFLAGS = -Wall -Werror $(DFLAGS)
SRCS := $(wildcard *.c)
SRCS += $(wildcard *.h)

all: examples

examples:
	$(CC) 
