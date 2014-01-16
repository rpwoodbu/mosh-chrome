// Override assert.h to make Mosh build against newlib.
// This includes things not necessarily meant for assert.h, but this is a
// convenient place that is included from most things.

#ifndef __OVERRIDE_ASSERT_H__
#define __OVERRIDE_ASSERT_H__

// First, include the "real" assert.h, through a symlink maintained by the
// build script.
#include "../../build/include/assert.h"

// Include anything that wasn't included properly.
#include <sys/time.h>

extern "C" {

// Newlib headers are missing posix_memalign().
int posix_memalign(void **memptr, size_t alignment, size_t size);

// Define getrlimit() and friends, which we "implement" in the wrapper.
#define RLIMIT_CORE 0
typedef int rlim_t;
struct rlimit {
  rlim_t rlim_cur;  /* Soft limit */
  rlim_t rlim_max;  /* Hard limit (ceiling for rlim_cur) */
};
int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

// Define some things needed for Internet functions.
typedef uint32_t u_int32_t;
// TODO: Make this do something better and just ignore alignment.
#define _ALIGN(n) n

// Nullify cfmakeraw, as we don't need it to function.
#define cfmakeraw(n) ;

// Define pselect.
#include <sys/signal.h>
extern int pselect (int __nfds, fd_set *__restrict __readfds,
    fd_set *__restrict __writefds,
    fd_set *__restrict __exceptfds,
    const struct timespec *__restrict __timeout,
    const sigset_t *__restrict __sigmask);

// Newlib's getopt() seems to crash. Redirect it to our implementation.
#define getopt(a, b, c) mygetopt(a, b, c)
int mygetopt(int argc, char * const argv[], const char *optstring);

} // extern "C"

#endif // __OVERRIDE_ASSERT_H__
