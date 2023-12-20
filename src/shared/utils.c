/*
Author: Totto16

*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// simple malloc Wrapper, using also memset to set everything to 0
void* mallocOrFail(const size_t size, const bool initializeWithZeros) {
	void* result = malloc(size);
	if(result == NULL) {
		fprintf(stderr, "ERROR: Couldn't allocate memory!\n");
		exit(EXIT_FAILURE);
	}
	if(initializeWithZeros) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(result, 0, size);
		if(result != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory allocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			exit(EXIT_FAILURE);
		}
	}
	return result;
}

// simple realloc Wrapper, using also memset to set everything to 0
void* reallocOrFail(void* previousPtr, const size_t oldSize, const size_t newSize,
                    const bool initializeWithZeros) {
	void* result = realloc(previousPtr, newSize);
	if(result == NULL) {
		fprintf(stderr, "ERROR: Couldn't reallocate memory!\n");
		exit(EXIT_FAILURE);
	}
	if(initializeWithZeros && newSize > oldSize) {
		// yes this could be done by calloc, but if you don't need that, its overhead!
		void* secondResult = memset(((char*)result) + oldSize, 0, newSize - oldSize);
		if(((char*)result) + oldSize != secondResult) {
			// this shouldn't occur, but "better be safe than sorry"
			fprintf(stderr, "FATAL: Couldn't set the memory reallocated to zeros!!\n");
			// free not really necessary, but also not that wrong
			free(result);
			exit(EXIT_FAILURE);
		}
	}
	return result;
}

// copied from exercises before (PS 1-8, selfmade), it safely parses a long!
long parseLongSafely(const char* toParse, const char* description) {
	// this is just allocated, so that strtol can write an address into it,
	// therefore it doesn't need to be initialized
	char* endpointer;
	// reseting errno, since it's not guaranteed to be that, but strtol can return some values that
	// generally are also valid, so errno is the only REAL and consistent method of checking for
	// error
	errno = 0;
	// using strtol, string to long, since atoi doesn't report errors that well
	long result = strtol(toParse, &endpointer, 10);

	// it isn't a number, if either errno is set or if the endpointer is not a '\0
	if(*endpointer != '\0') {
		fprintf(stderr, "ERROR: Couldn't parse the incorrect long %s for the argument %s!\n",
		        toParse, description);
		exit(EXIT_FAILURE);
	} else if(errno != 0) {
		perror("Couldn't parse the incorrect long");
		exit(EXIT_FAILURE);
	}

	return result;
}
