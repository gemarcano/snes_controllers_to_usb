// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <sctu/log.h>
#include <sctu/usb.h>
#include <sctu/cli_task.h>
#include <sctu/watchdog.h>
#include <sctu/cdc_device.h>
#include <sctu/controller.h>
#include <sctu/pio_controllers.h>

#include <hardware/structs/mpu.h>

#include <tusb_config.h>
#include <tusb.h>
#include <bsp/board_api.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <cstdio>

using sctu::sys_log;

static void print_callback(std::string_view str)
{
	printf("syslog: %.*s\r\n", str.size(), str.data());
}

constexpr const std::array<int, 4> led_gpios { 14, 15, 16, 17 };

// FreeRTOS task to handle polling controller and sending HID reports
static void hid_task(void*)
{
	TickType_t last = xTaskGetTickCount();
	sctu::pio_controllers controllers(pio0);

	// Initialize LED GPIOs
	for (int led: led_gpios)
	{
		gpio_init(led);
		gpio_set_dir(led, true);
	}

	std::array<sctu::controller, 4> last_state = {};
	for (;;)
	{
		vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
		const std::array<sctu::controller, 4> state = controllers.poll();
		// Keep track of the number of controllers configured, used to index
		// tinyusb devices
		uint8_t controller_ready = 0;
		for (uint8_t i = 0; i < 4; ++i)
		{
			// Update USB controller state if there's a change
			if (last_state[i].connected != state[i].connected)
			{
				if (state[i].connected)
					usb_enable_controller(1 << i);
				else
					usb_disable_controller(1 << i);
				gpio_put(led_gpios[i], state[i].connected);
			}

			// Only bother updating the tinyusb report if tinyusb is ready and
			// we're connected.
			if (state[i].connected && tud_hid_n_ready(controller_ready))
			{
				// Remote wakeup only if it's suspended, and a button is pressed
				if (tud_suspended() && (state[i].x || state[i].y || state[i].buttons))
				{
					// Host must allow waking up from this device for this to work
					tud_remote_wakeup();
				}

				// Only send a report if the data has changed
				else if (last_state[i] != state[i])
				{
					// Our report only has 3 bytes, don't assume the struct with the data has
					// no padding, and don't use the no padding directive for structs-- last
					// thing I want to deal with is misaligned data access on ARM
					uint8_t buffer[3];
					buffer[0] = state[i].x;
					buffer[1] = state[i].y;
					buffer[2] = state[i].buttons;
					tud_hid_n_report(controller_ready, 0, &buffer, sizeof(buffer));
				}
				controller_ready++;
			}
			last_state[i] = state[i];
		}
	}
}

// FreeRTOS task to handle USB tasks
static void usb_device_task(void*)
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

static void initialize_mpu()
{
	mpu_hw->ctrl = 5; // enable mpu with background default map
	mpu_hw->rbar = (0x0 & ~0xFFu)| M0PLUS_MPU_RBAR_VALID_BITS | 0;
	mpu_hw->rasr =
		1             // enable region
		| (0x7 << 1)  // size 2^(7 + 1) = 256
		| (0 << 8)    // Subregion disable-- don't disable any
		| 0x10000000; // Disable instruction fetch, disallow all
}

static void init_cpu_task(void* val)
{
	std::atomic_bool *cpu_init = reinterpret_cast<std::atomic_bool*>(val);
	initialize_mpu();
	*cpu_init = true;
	vTaskDelete(nullptr);
	for(;;);
}

static void init_task(void*)
{
	std::array<std::atomic_bool, 2> cpu_init = {};
	xTaskCreateAffinitySet(
		init_cpu_task,
		"sctu_cpu0_init",
		configMINIMAL_STACK_SIZE,
		&cpu_init[0],
		tskIDLE_PRIORITY+1,
		1 << 0,
		nullptr);

	xTaskCreateAffinitySet(
		init_cpu_task,
		"sctu_cpu1_init",
		configMINIMAL_STACK_SIZE,
		&cpu_init[1],
		tskIDLE_PRIORITY+1,
		1 << 1,
		nullptr);

	// Wait until CPU init tasks are done
	while(!cpu_init[0] || !cpu_init[1])
	{
		taskYIELD();
	}

	// We're not calling board_init() since for our configuration, all it is
	// really doing is initializing UART, which... we're not using at all.
	sctu::initialize_watchdog_tasks();
	sys_log.register_push_callback(print_callback);

	// Anything USB related needs to be on the same core-- just use core 2
	xTaskCreateAffinitySet(
		usb_device_task,
		"sctu_usb",
		configMINIMAL_STACK_SIZE*2,
		nullptr,
		tskIDLE_PRIORITY+1,
		1 << 1,
		nullptr);

	xTaskCreateAffinitySet(
		hid_task,
		"sctu_controller",
		configMINIMAL_STACK_SIZE,
		nullptr,
		tskIDLE_PRIORITY+1,
		1 << 1,
		nullptr);

	// CLI doesn't need to be in the same core as USB...
	xTaskCreateAffinitySet(
		sctu::cli_task,
		"sctu_cli",
		configMINIMAL_STACK_SIZE*2,
		nullptr,
		tskIDLE_PRIORITY+1,
		1 << 0,
		nullptr);

	// ...and kill this init task as it's done.
	vTaskDelete(nullptr);
	for(;;);
}

int main()
{
	// Alright, based on reading the pico-sdk, it's pretty much just a bad idea
	// to do ANYTHING outside of a FreeRTOS task when using FreeRTOS with the
	// pico-sdk... just do all required initialization in the init task
	xTaskCreateAffinitySet(
		init_task,
		"sctu_init",
		configMINIMAL_STACK_SIZE,
		nullptr,
		tskIDLE_PRIORITY+1,
		(1 << 0) | (1 << 1),
		nullptr);

	vTaskStartScheduler();
	for(;;);
	return 0;
}
