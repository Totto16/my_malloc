# My Malloc

## What is this

A Custom Malloc implementation using a best fit allocator with a in memory double linked list implementation, that stores status (free, allocated) information.

This conforms mostly to POSIX `malloc`, `realloc`, `free` specification, but for more details, see the function documentation.

It is fully thread safe (with a mutex by default), you can turn it off, if you like the minimal additional speed and can assure, that it's not used in MT context.

It also implements a thread_local variant, sop that each thread has it's own `mmmap`'ed storage pool. 

## Additional things

The `my_malloc`, `my_realloc`, `my_free` functions all define valgrind compatible blocks, so if you have valgrind headers installed, it uses those and you can run the programm with valgrind, to check for memory leeks.  


## How to build

### Prerequisite

You need certain things for this to work:

- meson - install from [here](https://mesonbuild.com/Quick-guide.html#installation-using-python) 
- A C2x std compatible compiler (e.g. GCC)
- A C++ compatible compiler (onyl for tests with gtest)



### How to run


```bash
meson setup build
meson compile -C build
meson test -C build --verbose # for tests
./build/src/task2/tests_with_double_pointers --all
```
