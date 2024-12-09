#ifndef LEVENSHTEIN_H
#define LEVENSHTEIN_H

#include <stddef.h>

// `levenshtein.h` - levenshtein
// MIT licensed.
// Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>

// Returns a size_t, depicting the difference between `a` and `b`.
// See <https://en.wikipedia.org/wiki/Levenshtein_distance> for more information.
#ifdef __cplusplus
extern "C" {
#endif

size_t
levenshtein(const char *__restrict__ a, const char *__restrict__ b);

size_t
levenshtein_n(const char *__restrict__ a, const size_t length, const char *__restrict__ b, const size_t bLength);

#ifdef __cplusplus
}
#endif

#endif // LEVENSHTEIN_H
