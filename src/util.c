

#include "types.h"

#ifdef WIN32
#include <windows.h>

long GetTimeMS() { return GetTickCount(); }

#else
#include <stddef.h>
#include <sys/time.h>

long GetTimeMS() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

#endif
