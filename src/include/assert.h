// Override assert.h to make Mosh build against newlib.
// This includes things not necessarily meant for assert.h, but this is a
// convenient place that is included from most things.

#ifndef __OVERRIDE_ASSERT_H__
#define __OVERRIDE_ASSERT_H__

// First, include the "real" assert.h, through a symlink maintained by the
// build script.
#include "../../build/include/assert.h"

// Include anything we want later to override.
#include <unistd.h>

extern "C" {

// Override dup() from unistd.h.
#undef dup
#define dup(x) __wrap_dup(x)
int __wrap_dup(int);

// Define pselect.
#include <sys/signal.h>
extern int pselect (int __nfds, fd_set *__restrict __readfds,
    fd_set *__restrict __writefds,
    fd_set *__restrict __exceptfds,
    const struct timespec *__restrict __timeout,
    const sigset_t *__restrict __sigmask);

} // extern "C"

#endif // __OVERRIDE_ASSERT_H__
