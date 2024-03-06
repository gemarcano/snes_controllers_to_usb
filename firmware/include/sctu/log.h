// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_LOG_H_
#define SCTU_LOG_H_

#include <sctu/syslog.h>

namespace sctu
{
	extern safe_syslog<syslog<1024*128>> sys_log;
}

#endif//SCTU_LOG_H_
