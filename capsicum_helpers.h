/*-
 * Copyright (c) 2016 Mariusz Zaborski <oshogbo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CAPSICUM_HELPERS_H_
#define	_CAPSICUM_HELPERS_H_

#include <sys/param.h>
#include <sys/capsicum.h>

#include <errno.h>
#include <nl_types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define	CAPH_IGNORE_EBADF	0x0001
#define	CAPH_READ		0x0002
#define	CAPH_WRITE		0x0004

static inline int
caph_limit_fd(int fd, int flags)
{
	cap_rights_t rights;
	unsigned long cmds[] = { TIOCGETA, TIOCGWINSZ };

	cap_rights_init(&rights, CAP_FCNTL, CAP_FSTAT, CAP_IOCTL);

	if ((flags & CAPH_READ) == CAPH_READ)
		cap_rights_set(&rights, CAP_READ);
	if ((flags & CAPH_WRITE) == CAPH_WRITE)
		cap_rights_set(&rights, CAP_WRITE);

	if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS) {
		if (errno == EBADF && (flags & CAPH_IGNORE_EBADF) != 0)
			return (0);
		return (-1);
	}

	if (cap_ioctls_limit(fd, cmds, nitems(cmds)) < 0 && errno != ENOSYS)
		return (-1);

	if (cap_fcntls_limit(fd, CAP_FCNTL_GETFL) < 0 && errno != ENOSYS)
		return (-1);

	return (0);
}

static inline int
caph_limit_stdin(void)
{

	return (caph_limit_fd(STDIN_FILENO, CAPH_READ));
}

static inline int
caph_limit_stderr(void)
{

	return (caph_limit_fd(STDERR_FILENO, CAPH_WRITE));
}

static inline int
caph_limit_stdout(void)
{

	return (caph_limit_fd(STDOUT_FILENO, CAPH_WRITE));
}

static inline int
caph_limit_stdio(void)
{

	if (caph_limit_stdin() == -1 || caph_limit_stdout() == -1 ||
	    caph_limit_stdout() == -1) {
		return (-1);
	}

	return (0);
}

static inline void
caph_cache_tzdata(void)
{

	tzset();
}

static inline void
caph_cache_catpages(void)
{

	(void)catopen("libc", NL_CAT_LOCALE);
}

#endif /* _CAPSICUM_HELPERS_H_ */
