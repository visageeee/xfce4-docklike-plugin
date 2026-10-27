// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cerrno>
#include <csignal>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

namespace ProcessStop
{
// A pidfd identifies this process even if its PID is later reused.
inline int open(int pid)
{
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
	if (pid <= 1 || pid == getpid()) { errno = EPERM; return -1; }
	const int fd = syscall(SYS_pidfd_open, pid, 0);
	if (fd < 0) return -1;
	// Let the kernel check signal permissions on the process handle itself.
	// This also works when /proc is mounted from a different PID namespace.
	if (syscall(SYS_pidfd_send_signal, fd, 0, nullptr, 0) != 0)
	{
		const int error = errno;
		::close(fd); errno = error; return -1;
	}
	return fd;
#else
	(void)pid; errno = ENOSYS; return -1;
#endif
}

inline bool signal(int fd, int number)
{
#if defined(SYS_pidfd_send_signal)
	return syscall(SYS_pidfd_send_signal, fd, number, nullptr, 0) == 0;
#else
	(void)fd; (void)number; errno = ENOSYS; return false;
#endif
}
} // namespace ProcessStop
