// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <tusb.h>
#include <cdc.h>

#include <cerrno>
#include <span>
#include <cstdint>
#include <atomic>
#include <array>
#include <cstdio>

#include <errno.h>
#undef errno
extern int errno;

#include <pico/rand.h>

// Just hang if we call a pure virtual function, the default implementation
// relies on too much stuff
extern "C" void __cxa_pure_virtual() { for(;;); }

// Also just hang if we get to the __verbose_terminate_handler
namespace __gnu_cxx
{
	void __verbose_terminate_handler()
	{
		for (;;);
	}
}

namespace sctu
{
	cdc_device cdc;
}

// Apparently, if I don't declare this function as used, LTO gets rid of it...
extern "C" int _write(int fd, char *buf, int count) __attribute__ ((used));
extern "C" int _write(int fd, char *buf, int count)
{
	if (fd == 0 || fd > 3)
	{
		errno = EBADF;
		return -1;
	}

	sctu::io_device& dev = sctu::cdc;
	return dev.write(std::span<const unsigned char>(reinterpret_cast<unsigned char*>(buf), count));
}

extern "C" int _read(int fd, char *buf, int count) __attribute__ ((used));
extern "C" int _read(int fd, char *buf, int count)
{
	if (fd != 3 && fd >= 1)
	{
		errno = EBADF;
		return -1;
	}

	sctu::io_device& dev = sctu::cdc;
	return dev.read(std::span<unsigned char>(reinterpret_cast<unsigned char*>(buf), count));
}

extern "C" int _open(const char *name, int flags, int mode) __attribute__ ((used));
extern "C" int _open(const char *name, int /*flags*/, int /*mode*/)
{
	// FIXME do I want to do with anything with flags and mode???
	if (strcmp(name, "cdc") == 0)
	{
		return 3;
	}

	errno = ENOENT;
	return -1;
}

extern "C" int _close(int fd) __attribute__ ((used));
extern "C" int _close(int fd)
{
	if (fd == 3)
	{
		return 0;
	}
	errno = EBADF;
	return -1;
}

extern "C" int getentropy(void *buffer, size_t length) __attribute__ ((used));
extern "C" int getentropy(void *buffer, size_t length)
{
	uint32_t random = get_rand_32();
	size_t i;
	for (i = 0; i < length - 3; i += 4)
	{
		memcpy(reinterpret_cast<unsigned char*>(buffer)+i, &random, 4);
		random = get_rand_32();
	}
	memcpy(reinterpret_cast<unsigned char*>(buffer) + i - 4, &random, length % 4);
	return 0;
}
