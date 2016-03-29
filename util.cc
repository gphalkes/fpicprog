#include "util.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

void Sleep(Duration duration) {
	if (duration <= 0) return;
	struct timespec to_sleep;
	to_sleep.tv_sec = duration / 1000000000;
	to_sleep.tv_nsec = duration % 1000000000;
	while (nanosleep(&to_sleep, &to_sleep)) {
		if (errno != EINTR)
			FATAL("nanosleep failed: %s\n", strerror(errno));
	}
}
