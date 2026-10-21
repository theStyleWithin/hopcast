/* HopperCast debug builds: write a backtrace to the session log when the app crashes.
 * Records only module names and code offsets - no memory contents, so no stream keys.
 * GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void on_crash(int sig)
{
	static const char head[] = "\n=== HOPPERCAST CRASH ===\nsignal: ";
	char num[16];
	void *frames[64];
	int fd = 2, n, i = 0, v = sig;
	const char *path = getenv("HOPCAST_CRASH_LOG");
	if (path) {
		int f = open(path, O_WRONLY | O_APPEND);
		if (f >= 0)
			fd = f;
	}
	write(fd, head, sizeof head - 1);
	if (v == 0) num[i++] = '0';
	char tmp[12]; int t = 0;
	while (v > 0) { tmp[t++] = '0' + v % 10; v /= 10; }
	while (t) num[i++] = tmp[--t];
	num[i++] = '\n';
	write(fd, num, i);
	n = backtrace(frames, 64);
	backtrace_symbols_fd(frames, n, fd);
	write(fd, "=== END ===\n", 12);
	signal(sig, SIG_DFL);
	raise(sig);
}

__attribute__((constructor)) static void init(void)
{
	static char stack[65536];
	stack_t ss = {.ss_sp = stack, .ss_size = sizeof stack};
	struct sigaction sa;
	int sigs[] = {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE};
	void *warm[1];
	backtrace(warm, 1); /* load libgcc now, not inside the handler */
	sigaltstack(&ss, NULL);
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_crash;
	sa.sa_flags = SA_ONSTACK | SA_RESETHAND;
	for (unsigned i = 0; i < sizeof sigs / sizeof *sigs; i++)
		sigaction(sigs[i], &sa, NULL);
	unsetenv("LD_PRELOAD"); /* don't leak into helper processes */
}
