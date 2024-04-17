CC = gcc
CFLAGS = -g -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS = -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup

.PHONY: all clean

memory_tests.o: memory_tests.c
nand.o: nand.c nand.h
testy.o: testy.c

testy: testy.o libnand.so
	gcc -L. -o $@ $< -lnand

nand_example: nand_example.c memory_tests.o libnand.so
	gcc -L. -o $@ $^ -lnand

libnand.so: nand.o
	gcc -shared -o $@ $^

all: libnand.so nand_example testy

clean:
	rm -f *.o *.so