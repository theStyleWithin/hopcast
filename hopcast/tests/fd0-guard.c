/* HopCast audit helper (LD_PRELOAD): report who closes standard descriptors.
 * Wraps close() and close_range(); when fd 0, 1 or 2 is closed it prints a
 * backtrace to the log named in HOPCAST_FD_GUARD_LOG, then closes as normal.
 * Build: gcc -shared -fPIC -O1 -o fd0-guard.so fd0-guard.c -ldl
 * GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

static void report(const char *what, int fd)
{
	const char *path = getenv("HOPCAST_FD_GUARD_LOG");
	if (!path)
		return;
	int out = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (out < 0)
		return;
	char head[96];
	int n = snprintf(head, sizeof head, "=== %s(%d) pid=%d tid=%d ===\n", what, fd, getpid(), gettid());
	if (n > 0)
		(void)!write(out, head, (size_t)n);
	void *frames[40];
	int count = backtrace(frames, 40);
	backtrace_symbols_fd(frames, count, out);
	syscall(SYS_close, out);
}

int close(int fd)
{
	static int (*real_close)(int);
	if (!real_close)
		real_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
	if (fd >= 0 && fd <= 2)
		report("close", fd);
	return real_close(fd);
}

int close_range(unsigned int first, unsigned int last, int flags)
{
	static int (*real_cr)(unsigned int, unsigned int, int);
	if (!real_cr)
		real_cr = (int (*)(unsigned int, unsigned int, int))dlsym(RTLD_NEXT, "close_range");
	if (first <= 2)
		report("close_range", (int)first);
	return real_cr ? real_cr(first, last, flags) : -1;
}
