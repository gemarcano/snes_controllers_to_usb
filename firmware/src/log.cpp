// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <sctu/log.h>
#include <sctu/syslog.h>

namespace sctu
{
	safe_syslog<syslog<1024*128>> sys_log;
}
