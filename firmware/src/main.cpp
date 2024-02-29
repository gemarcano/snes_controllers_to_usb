// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <log.h>
#include <server.h>
#include <cdc.h>
#include <controller.h>
#include <wifi_management_task.h>
#include <network_task.h>
#include <cli_task.h>
#include <usb.h>
#include <watchdog.h>
#include <pio_controllers.h>

#include <hardware/structs/mpu.h>

#include <lwip/netdb.h>

#include <tusb_config.h>
#include <tusb.h>
#include <bsp/board_api.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <cstdio>
#include <random>

using sctu::sys_log;

void print_callback(std::string_view str)
{
	printf("syslog: %.*s\r\n", str.size(), str.data());
}

class random_controller : public sctu::controller
{
public:
	sctu::controller_state poll() override
	{
		return sctu::controller_state{true, static_cast<int8_t>(dist(rdev)), static_cast<int8_t>(dist(rdev)), dist(rdev)};
	}

private:
	std::random_device rdev;
	std::uniform_int_distribution<uint8_t> dist{0u, 255u};
};

// FreeRTOS task to handle polling controller and sending HID reports
void hid_task(void *)
{
	TickType_t last = xTaskGetTickCount();
	sctu::pio_controllers controllers(pio0);

	gpio_init_mask(
		(1u << 14) |
			(1u << 15) |
			(1u << 16) |
			(1u << 17)
	);
	gpio_set_dir_out_masked(
		(1u << 14) |
			(1u << 15) |
			(1u << 16) |
			(1u << 17)
	);

	// FIXME wait for USB initialization
	//
	std::array<sctu::controller_state, 4> last_state = {};
	for (;;)
	{
		vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
		std::array<sctu::controller_state, 4> state = controllers.poll();
		uint8_t controller_ready = 0;
		for (uint8_t i = 0; i < 4; ++i)
		{
			if (last_state[i].connected != state[i].connected)
			{
				if (state[i].connected)
					usb_enable_controller(1 << i);
				else
					usb_disable_controller(1 << i);
				gpio_put(14 + i, state[i].connected);
			}

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

void init_task(void*)
{
	board_init();

	// This is running on core 2, setup MPU to catch null pointer dereferences
	initialize_mpu();

	TaskHandle_t handle;

	sctu::initialize_watchdog_tasks();

	sys_log.register_push_callback(print_callback);

	// Anything USB related needs to be on the same core-- just use core 2
	xTaskCreate(usb_device_task, "usb", 512, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(sctu::cli_task, "prb_cli", 512, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(hid_task, "controller", 512*2, nullptr, tskIDLE_PRIORITY+1, &handle);
	vTaskCoreAffinitySet(handle, (1 << 1) );
	xTaskCreate(sctu::wifi_management_task, "wifi", 512*4, nullptr, tskIDLE_PRIORITY+1, &handle);
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
	initialize_mpu();

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
