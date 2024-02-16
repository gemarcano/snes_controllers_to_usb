// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <log.h>
#include <server.h>
#include <syslog.h>
#include <tusb_config.h>
#include <cdc.h>

#include <pico/unique_id.h>
#include <pico/cyw43_arch.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>
#include "hardware/structs/mpu.h"

#include <lwip/netdb.h>

#include <tusb.h>
#include <bsp/board_api.h>

extern "C" {
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
}

#include <cstring>
#include <ctime>
#include <memory>
#include <expected>
#include <atomic>
#include <algorithm>
#include <random>

// This secrets.h includes strings for WIFI_SSID and WIFI_PASSWORD
#include "secrets.h"

#include <network_task.h>
#include <cli_task.h>

using sctu::sys_log;

void status_callback(netif *)
{
	sys_log.push("status changed, trying to reconnect");
	cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
	while (!cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
		sys_log.push("FAILED to reconnect, trying again");
	}
}

void link_callback(netif *)
{
	sys_log.push("link changed");
}

void print_callback(std::string_view str)
{
	printf("syslog: %.*s\r\n", str.size(), str.data());
}

void watchdog_task(void*)
{
	// The watchdog period needs to be long enough so long lock periods
	// (apparently something in the wifi subsystem holds onto a lock for a
	// while) are tolerated.
	watchdog_enable(200, true);
	for(;;)
	{
		watchdog_update();
		vTaskDelay(50);
	}
}

struct controller_state
{
	int8_t x;        // x axis, [-127, 127]
	int8_t y;        // y axis, [-127, 127]
	uint8_t buttons; // buttons, 8 buttons
	bool operator==(const controller_state&) const = default;
};

// FreeRTOS task to handle polling controller and sending HID reports
void hid_task(void *)
{
	TickType_t last = xTaskGetTickCount();
	controller_state last_data{};

	std::random_device rdev;
	std::uniform_int_distribution<uint8_t> dist(0u, 255u);
	// FIXME wait for USB initialization

	for (;;)
	{
		vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
		if (tud_hid_n_ready(0))
		{
			controller_state data = {static_cast<int8_t>(dist(rdev)), static_cast<int8_t>(dist(rdev)), dist(rdev)};//controller.read();

			// Remote wakeup only if it's suspended, and a button is pressed
			if (tud_suspended() && (data.x || data.y || data.buttons))
			{
				// Host must allow waking up from this device for this to work
				tud_remote_wakeup();
			}

			// Only send a report if the data has changed
			else if (last_data != data)
			{
				// Our report only has 3 bytes, don't assume the struct with the data has
				// no padding, and don't use the no padding directive for structs-- last
				// thing I want to deal with is misaligned data access on ARM
				uint8_t buffer[3];
				buffer[0] = data.x;
				buffer[1] = data.y;
				buffer[2] = data.buttons;
				tud_hid_n_report(0, 0, &buffer, sizeof(buffer));
				last_data = data;
			}
		}

		if (tud_hid_n_ready(1))
		{
			controller_state data = {static_cast<int8_t>(dist(rdev)), static_cast<int8_t>(dist(rdev)), dist(rdev)};//controller.read();

			// Remote wakeup only if it's suspended, and a button is pressed
			if (tud_suspended() && (data.x || data.y || data.buttons))
			{
				// Host must allow waking up from this device for this to work
				tud_remote_wakeup();
			}

			// Only send a report if the data has changed
			else if (last_data != data)
			{
				// Our report only has 3 bytes, don't assume the struct with the data has
				// no padding, and don't use the no padding directive for structs-- last
				// thing I want to deal with is misaligned data access on ARM
				uint8_t buffer[3];
				buffer[0] = data.x;
				buffer[1] = data.y;
				buffer[2] = data.buttons;
				tud_hid_n_report(1, 0, &buffer, sizeof(buffer));
				last_data = data;
			}
		}
	}
}

// FreeRTOS task to handle USB tasks
void usb_device_task(void *)
{
	tusb_init();
	for(;;)
	{
		tud_task();
		// tud_cdc_connected() must be called in the same task as tud_task, as
		// an internal data structure is shared without locking between both
		// functions. See https://github.com/hathach/tinyusb/issues/1472
		// As a workaround, use an atomic variable to get the result of this
		// function, and read from it elsewhere
		sctu::cdc.update();
		taskYIELD();
	}
}

void init_wifi(void*)
{
	// Loop indefinitely until we connect to WiFi
	size_t retry = 0;
	for (;retry < 10; ++retry)
	{
		sys_log.push("Initializing cyw43 with USA region...: ");
		// cyw43_arch_init _must_ be called within a FreeRTOS task, see
		// https://github.com/raspberrypi/pico-sdk/issues/1540
		if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
		{
			sys_log.push("    FAILED");
			continue;
		}
		// Turn off powersave completely
		cyw43_wifi_pm(&cyw43_state, CYW43_DEFAULT_PM & ~0xf);
		sys_log.push("    DONE");
		cyw43_arch_enable_sta_mode();

		sys_log.push(std::format("Connecting to SSID {}:", WIFI_SSID));
		if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
			sys_log.push("    FAILED");
			continue;
		}
		sys_log.push("    DONE");

		cyw43_arch_lwip_begin();
		netif_set_status_callback(netif_default, status_callback);
		netif_set_link_callback(netif_default, link_callback);
		cyw43_arch_lwip_end();

		break;
	}

	if (retry == 10)
	{
			sys_log.push("We failed to connect, attempting to kill and reset...");
			TaskHandle_t handle;
			handle = xTaskGetHandle("watchdog");
			vTaskDelete(handle);
			for(;;);
	}

	sys_log.push(std::format("Connected with IP address {}", ip4addr_ntoa(netif_ip4_addr(netif_default))));

	// FIXME should we call this somewhere?
	//cyw43_arch_deinit();

	TaskHandle_t handle;
	xTaskCreate(sctu::network_task, "prb_network", 512, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	vTaskDelete(nullptr);
	for(;;);
}

void init_task(void*)
{
	board_init();

	// This is running on core 2, setup MPU to catch null pointer dereferences
	mpu_hw->ctrl = 5; // enable mpu with background default map
	mpu_hw->rbar = (0x0 & ~0xFFu)| M0PLUS_MPU_RBAR_VALID_BITS | 0;
	mpu_hw->rasr =
		1             // enable region
		| (0x7 << 1)  // size 2^(7 + 1) = 256
		| (0 << 8)    // Subregion disable-- don't disable any
		| 0x10000000; // Disable instruction fetch, disallow all

	// Initialize watchdog hardware
	TaskHandle_t handle;
	TaskHandle_t watch_handle;
	// Watchdog priority is higher!
	xTaskCreate(watchdog_task, "watchdog", 256, nullptr, tskIDLE_PRIORITY+2, &watch_handle);
	vTaskCoreAffinitySet(watch_handle, (1 << 1) );
	sys_log.register_push_callback(print_callback);

	xTaskCreate(usb_device_task, "usb", 512, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(sctu::cli_task, "prb_cli", 512, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(hid_task, "controller", 512*2, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(init_wifi, "wifi", 512*4, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );

	vTaskDelete(nullptr);
	for(;;);
}

extern "C" void vApplicationStackOverflowHook(
	TaskHandle_t /*xTask*/, char * /*pcTaskName*/)
{
	__asm__ __volatile__ ("bkpt #0");
}

int main()
{
	// This is running on core 1, setup MPU to catch null pointer dereferences
	mpu_hw->ctrl = 5; // enable mpu with background default map
	mpu_hw->rbar = (0x0 & ~0xFFu)| M0PLUS_MPU_RBAR_VALID_BITS | 0;
	mpu_hw->rasr =
		1             // enable region
		| (0x7 << 1)  // size 2^(7 + 1) = 256
		| (0 << 8)    // Subregion disable-- don't disable any
		| 0x10000000; // Disable instruction fetch, disallow all

	// Alright, based on reading the pico-sdk, it's pretty much just a bad idea
	// to do ANYTHING outside of a FreeRTOS task when using FreeRTOS with the
	// pico-sdk... just do all required initialization in the init task
	TaskHandle_t init_handle;
	xTaskCreate(init_task, "prb_init", 512*4, nullptr, tskIDLE_PRIORITY+1, &init_handle);
	vTaskCoreAffinitySet(init_handle, (1 << 1) );
	vTaskStartScheduler();
	for(;;);
	return 0;
}
