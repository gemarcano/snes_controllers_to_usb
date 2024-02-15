// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef LOG_H_
#define LOG_H_

#include <syslog.h>

namespace sctu
{
	extern safe_syslog<syslog<1024*128>> sys_log;
}

#endif//LOG_H_
