CC = gcc
CFLAGS = -D_GNU_SOURCE  -std=c11 -Wall -Wextra -Wpedantic -pthread
OFLAGS= -O3
LDFLAGS= -pthread

.PHONY: all
all: tests clean_o

.PHONY: clean
clean: clean_o
	$(RM) tests

clean_o:
	$(RM) *.o
	$(RM) ../tests/*.o
	$(RM) ../shared/*.o

tests: executable.o my_malloc.o ../shared/utils.o ../tests/allocator_tests.o ../tests/membench.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(OFLAGS) $(CFLAGS) -c -o $@ $^