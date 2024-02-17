// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <cli_task.h>
#include <log.h>
#include <usb.h>

#include <pico/unique_id.h>
#include <pico/cyw43_arch.h>
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
		printf("IP Address: %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
		printf("default instance: 0x%p\r\n", netif_default);
		printf("NETIF is up? %s\r\n", netif_is_up(netif_default) ? "yes" : "no");
		printf("NETIF flags: 0x%02X\r\n", netif_default->flags);
		printf("Wifi state: %d\r\n", cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA));

		int32_t rssi = 0;
		cyw43_wifi_get_rssi(&cyw43_state, &rssi);
		printf("  RSSI: %ld\r\n", rssi);
		uint32_t pm_state = 0;
		cyw43_wifi_get_pm(&cyw43_state, &pm_state);
		printf("power mode: 0x%08lX\r\n", pm_state);
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
			printf("log %u: %s\r\n", i, sys_log[i].c_str());
		}
	}

	if (line[0] == 'r')
	{
		printf("Rebooting...\r\n");
		fflush(stdout);
		reset_usb_boot(0,0);
	}

	if (line[0] == 'k')
	{
		printf("Killing (hanging)...\r\n");
		fflush(stdout);
		TaskHandle_t handle = xTaskGetHandle("watchdog");
		vTaskDelete(handle);
		for(;;);
	}

	if (line[0] == 't')
	{
		printf("Trying to kill usb to reload config?\r\n");
		tud_disconnect();
		printf("USB disconnected\r\n");
		vTaskDelay(5000);
		tud_connect();
		printf("USB reconnected\r\n");
	}

	if (line[0] == 'c')
	{
		printf("Current controllers: %d\r\n", get_active_controllers());
		unsigned controllers;
		sscanf(line + 2, "%u", &controllers);
		reset_configuration(controllers);
		vTaskDelay(1000);
		printf("Updated controllers: %d\r\n", get_active_controllers());
	}
}

namespace sctu
{
	void cli_task(void*)
	{
		char line[33] = {0};
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
					run(line);
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
