
// header guard
#ifndef _CUSTOM_UTILS_H_
#define _CUSTOM_UTILS_H_

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ( I know that having something like that is a HORRIBLE choice, but to confirm the exercise I can
// only use .h, and pasting everything in one c file is A MORE BAD idea than doing that with the .h
// files)

// uses snprintf feature with passing NULL,0 as first two arguments to automatically determine the
// required buffer size, for more read man page
// for variadic functions its easier to use macro
// magic, attention, use this function in the right way, you have to prepare a char* that is set to
// null, then it works best! snprintf is safer then sprintf, since it guarantees some things, also
// it has a failure indicator
#define formatString(toStore, format, ...) \
	{ \
		char* internalBuffer = *toStore; \
		if(internalBuffer != NULL) { \
			free(internalBuffer); \
		} \
		int toWrite = snprintf(NULL, 0, format, __VA_ARGS__) + 1; \
		internalBuffer = (char*)mallocOrFail(toWrite * sizeof(char), true); \
		int written = snprintf(internalBuffer, toWrite, format, __VA_ARGS__); \
		if(written >= toWrite) { \
			fprintf( \
			    stderr, \
			    "ERROR: Snprint did write more bytes then it had space in the buffer, available " \
			    "space:'%d', actually written:'%d'!\n", \
			    (toWrite)-1, written); \
			free(internalBuffer); \
			exit(EXIT_FAILURE); \
		} \
		*toStore = internalBuffer; \
	} \
	if(*toStore == NULL) { \
		fprintf(stderr, "ERROR: snprintf Macro gone wrong: '%s' is pointing to NULL!\n", \
		        #toStore); \
		exit(EXIT_FAILURE); \
	}

// simple error helper macro, with some more used "overloads"
#define checkForError(toCheck, errorString, statement) \
	do { \
		if((toCheck) == -1) { \
			fprintf(stderr, "%s: %s\n", errorString, strerror(errno)); \
			statement; \
		} \
	} while(false)

#define checkForThreadError(toCheck, errorString, statement) \
	do { \
		if((toCheck) != 0) { \
			/*pthread function don't set errno, but return the error value \
			 * directly*/ \
			fprintf(stderr, "%s: %s\n", errorString, strerror(toCheck)); \
			statement; \
		} \
	} while(false)

#define checkResultForThreadError(errorString, statement) \
	checkForThreadError(result, errorString, statement)

#define checkResultForThreadErrorAndReturn(errorString) \
	checkForThreadError(result, errorString, return EXIT_FAILURE;)

#define checkResultForThreadErrorAndExit(errorString) \
	checkForThreadError(result, errorString, exit(EXIT_FAILURE);)

#define checkResultForErrorAndExit(errorString) \
	checkForError(result, errorString, exit(EXIT_FAILURE););

#define printErrorAndExit(format, ...) \
	do { \
		fprintf(stderr, format, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while(false)

#define printSingleErrorAndExit(message) \
	do { \
		fprintf(stderr, message); \
		exit(EXIT_FAILURE); \
	} while(false)

// simple malloc Wrapper, using also memset to set everything to 0
void* mallocOrFail(const size_t size, const bool initializeWithZeros);

// simple realloc Wrapper, using also memset to set everything to 0
void* reallocOrFail(void* previousPtr, const size_t oldSize, const size_t newSize,
                    const bool initializeWithZeros);

// copied from exercises before (PS 1-8, selfmade), it safely parses a long!
long parseLongSafely(const char* toParse, const char* description);

// a hacky but good and understandable way that is used with pthread functions
// to annotate which type the really represent
#define any void*

#define anyType(type) /* Type helper for readability */ any

#if defined(_WITH_VALGRIND) && _WITH_VALGRIND == 1 && defined(_WANT_TO_USE_VALGRIND)

// IF Valgrind is included

#include <memcheck.h>
#include <valgrind.h>

#define VALGRIND_ALLOC(start_addr, size, padding, is_zeroed) \
	VALGRIND_MALLOCLIKE_BLOCK(start_addr, size, padding, is_zeroed)

#define VALGRIND_ALIGN_ALLOC_TO_GREATER_BLOCK(start_addr, size) \
	VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(start_addr, size)

#define VALGRIND_FREE(start, padding) VALGRIND_FREELIKE_BLOCK(start, padding)

#define MEMCHECK_DEFINE_INTERNAL_USE(start_addr, size) VALGRIND_MAKE_MEM_DEFINED(start_addr, size)

#define MEMCHECK_REMOVE_INTERNAL_USE(start_addr, size) VALGRIND_MAKE_MEM_UNDEFINED(start_addr, size)

#else

#define VALGRIND_ALLOC(a, b, c, d)
#define VALGRIND_FREE(a, b)
#define VALGRIND_ALIGN_ALLOC_TO_GREATER_BLOCK(a, b)
#define MEMCHECK_DEFINE_INTERNAL_USE(a, b)
#define MEMCHECK_REMOVE_INTERNAL_USE(a, b)

#endif

// HEADER GUARD END
#endif
