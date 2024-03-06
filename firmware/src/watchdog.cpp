// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <sctu/watchdog.h>

#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <ranges>

namespace sctu
{
	constexpr const int cpu_cores = 2;
	static std::array<std::atomic_bool, cpu_cores> watchdog_cpu_status;
	static std::array<const char*, cpu_cores> watchdog_task_names {
		"sctu_watchdog_cpu0",
		"sctu_watchdog_cpu1"
	};

	void watchdog_cpu_task(void* status)
	{
		for(;;)
		{
			std::atomic_bool &status_ =
				*reinterpret_cast<std::atomic_bool*>(status);
			status_ = true;
			vTaskDelay(50);
		}
	}

	static void watchdog_task(void*)
	{
		// The watchdog period needs to be long enough so long lock periods
		// (apparently something in the wifi subsystem holds onto a lock for a
		// while) are tolerated.
		watchdog_enable(200, true);
		for(;;)
		{
			// The compiler is apparently smart enough to optimize this by
			// unrolling the range check
			constexpr const auto value = [](bool val){return val;};
			if (std::ranges::all_of(watchdog_cpu_status, value))
			{
				watchdog_update();
				std::ranges::fill(watchdog_cpu_status, 0);
			}
			vTaskDelay(30);
		}
	}

	void initialize_watchdog_tasks()
	{
		// Watchdog priority is higher
		// Dedicated watchdog tasks on each core, and have a central watchdog task
		// aggregate the watchdog flags from both other tasks.
		// If one core locks up, the central task will detect it and not pet the
		// watchdog, or it will itself be hung, leading to a system reset.
		for (size_t i = 0; i < cpu_cores; ++i)
		{
			xTaskCreateAffinitySet(
				watchdog_cpu_task,
				watchdog_task_names[i],
				configMINIMAL_STACK_SIZE,
				&watchdog_cpu_status[i],
				tskIDLE_PRIORITY+2,
				1 << i,
				nullptr);
		}
		xTaskCreateAffinitySet(
			watchdog_task,
			"sctu_watchdog_core",
			configMINIMAL_STACK_SIZE,
			nullptr,
			tskIDLE_PRIORITY+2,
			(1 << 0) | (1 << 1),
			nullptr);
	}
}
