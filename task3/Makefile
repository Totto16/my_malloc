CC = gcc
CFLAGS = -D_GNU_SOURCE  -std=c11 -Wall -Wextra -Wpedantic -pthread
OFLAGS= -O3
LDFLAGS= -pthread


.PHONY: all
all:  free_list_thread_local best_fit_thread_local best_fit_without_bitfield best_fit_with_double_pointers clean_o

.PHONY: clean
clean: clean_o
	$(RM) free_list_thread_local best_fit_thread_local best_fit_without_bitfield best_fit_with_double_pointers

clean_o:
	$(RM) *.o
	$(RM) ../tests/*.o
	$(RM) ../shared/*.o
	$(RM) ../task1/*.o
	$(RM) ../task2/*.o

free_list_thread_local: executable_0.o ../task1/my_malloc_1.o ../shared/utils.o  ../tests/membench.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

best_fit_thread_local: executable_1.o ../task2/my_malloc_1.o ../shared/utils.o ../tests/membench.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

best_fit_without_bitfield: executable_1.o ../task2/my_malloc_without_bitfileds_1.o ../shared/utils.o ../tests/membench.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

best_fit_with_double_pointers: executable_1.o ../task2/my_malloc_with_double_pointers_1.o ../shared/utils.o ../tests/membench.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

executable_%.o: executable.c
	$(CC) -D_MALLOC=$* $(OFLAGS) $(CFLAGS) -c -o $@ $^


%my_malloc_1.o: %my_malloc.c
	$(CC) -D_PER_THREAD_ALLOCATOR=1 $(OFLAGS) $(CFLAGS)  -c -o $@ $^

%my_malloc_without_bitfileds_1.o: %my_malloc.c
	$(CC) -D_PER_THREAD_ALLOCATOR=1 -D_USE_BIFIELDS=0 $(OFLAGS) $(CFLAGS) -c -o $@ $^

%my_malloc_with_double_pointers_1.o: %my_malloc_with_pointers.c
	$(CC) -D_PER_THREAD_ALLOCATOR=1 -D_USE_POINTERS=2 -D_VALIDATE_BLOCKS=0 -D_PRINT_DEBUG=0 $(OFLAGS) $(CFLAGS) -c -o $@ $^


%.o: %.c
	$(CC) $(OFLAGS) $(CFLAGS) -c -o $@ $^