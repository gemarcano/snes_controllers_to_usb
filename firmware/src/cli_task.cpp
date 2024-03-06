// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <sctu/cli_task.h>
#include <sctu/log.h>
#include <sctu/usb.h>
#include <sctu/pio_controllers.h>

#include <pico/unique_id.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>
#include <tusb.h>

#include <cstdint>
#include <cstdio>
#include <algorithm>

using sctu::sys_log;

static void run(const char* line)
{
	if (line[0] == 's')
	{
		printf("ticks: %lu\r\n", xTaskGetTickCount());
		UBaseType_t number_of_tasks = uxTaskGetNumberOfTasks();
		printf("Tasks active: %lu\r\n", number_of_tasks);
		std::vector<TaskStatus_t> tasks(number_of_tasks);
		uxTaskGetSystemState(tasks.data(), tasks.size(), nullptr);
		for (const auto& status: tasks)
		{
			printf("  task name: %s\r\n", status.pcTaskName);
			printf("  task mark: %lu\r\n", status.usStackHighWaterMark);
		}

		char foo[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
		pico_get_unique_board_id_string(foo, sizeof(foo));
		printf("unique id: %s\r\n", foo);

		printf("log size: %u\r\n", sys_log.size());
		for (size_t i = 0; i < sys_log.size(); ++i)
		{
			printf("log %zu: %s\r\n", i, sys_log[i].c_str());
		}
	}

	if (line[0] == 'r')
	{
		printf("Rebooting to programming mode...\r\n");
		fflush(stdout);
		reset_usb_boot(0,0);
	}

	if (line[0] == 'k')
	{
		printf("Killing (hanging)...\r\n");
		fflush(stdout);
		// Just kill one of the watchdogs, should bring down the entire board
		TaskHandle_t handle = xTaskGetHandle("watchdog_cpu0");
		vTaskDelete(handle);
		for(;;);
	}

	if (line[0] == 'c')
	{
		printf("Current controllers: %01X\r\n", usb_get_active_controllers());
	}
}

namespace sctu
{
	void cli_task(void*)
	{
		std::array<char, 33> line = {0};
		unsigned int pos = 0;
		printf("> ");
		for(;;)
		{
			fflush(stdout);
			int c = fgetc(stdin);
			if (c != EOF)
			{
				if (c == '\r')
				{
					printf("\r\n");
					line[pos] = '\0';
					run(line.data());
					pos = 0;
					printf("> ");
					continue;
				}
				if (c == '\b')
				{
					if (pos > 0)
					{
						--pos;
						printf("\b \b");
					}
					continue;
				}

				if (pos < (sizeof(line) - 1))
				{
					line[pos++] = c;
					printf("%c", c);
				}
			}
			else
			{
				printf("WTF, we got an EOF?\r\n");
			}
		}
	}
}
