

#ifndef UTIL_H
#define UTIL_H

#include "types.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define stringize_(x) #x
#define stringize(x) stringize_(x)

#define INLINE static inline __attribute__((always_inline))

long GetTimeMS();

#endif