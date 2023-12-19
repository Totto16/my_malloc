CC = gcc
CFLAGS = -D_GNU_SOURCE  -std=c11 -Wall -Wextra -Wpedantic -pthread
OFLAGS= -O3 -g
LDFLAGS= -pthread

.PHONY: all
all: tests tests_without_bitfield tests_with_double_pointers clean_o

.PHONY: clean
clean: clean_o
	$(RM) tests tests_without_bitfield tests_with_double_pointers 

clean_o:
	$(RM) *.o
	$(RM) ../tests/*.o
	$(RM) ../shared/*.o

tests: executable.o my_malloc.o ../shared/utils.o ../tests/membench.o ../tests/allocator_tests.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

tests_without_bitfield: executable.o my_malloc_without_bitfileds.o ../shared/utils.o ../tests/membench.o ../tests/allocator_tests.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

tests_with_double_pointers : executable.o my_malloc_with_double_pointers.o ../shared/utils.o ../tests/membench.o ../tests/allocator_tests.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

my_malloc_without_bitfileds.o: my_malloc.c
	$(CC) -D_USE_BIFIELDS=0 $(OFLAGS) $(CFLAGS) -c -o $@ $^

my_malloc_with_double_pointers.o: my_malloc_with_pointers.c
	$(CC) -D_USE_POINTERS=2 -D_VALIDATE_BLOCKS=0 -D_PRINT_DEBUG=0 -D_IGNORE_SECURITY_CHECKS $(OFLAGS)  $(CFLAGS) -c -o $@ $^

%.o: %.c
	$(CC) $(OFLAGS) $(CFLAGS) -c -o $@ $^