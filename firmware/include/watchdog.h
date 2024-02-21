// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_WATCHDOG_H_
#define SCTU_WATCHDOG_H_

namespace sctu
{
	/** Initializes all of the watchdog tasks for the cores of the machine.
	 *
	 * For the rp2040, there are two cores, so each core gets its own dedicated
	 * task, plus one central task.
	 *
	 * This _must_ be called from within a FreeRTOS task!
	 */
	void initialize_watchdog_tasks();
}

#endif//SCTU_WATCHDOG_H_
