// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_PIO_CONTROLLER_H_
#define SCTU_PIO_CONTROLLER_H_

#include <controller.h>
#include <controllers.pio.h>

#include <hardware/pio.h>

#include <cstdint>

namespace sctu
{
	class pio_controllers
	{
	public:
		pio_controllers(const pio_controllers&) = delete;
		pio_controllers(pio_controllers&&) = delete;
		pio_controllers& operator=(const pio_controllers&) = delete;
		pio_controllers& operator=(pio_controllers&&) = delete;

		pio_controllers(PIO pio)
		:pio_(pio)
		{
			uint offset0 = pio_add_program(pio_, &controller0_program);
			uint offset1 = pio_add_program(pio_, &controllers1_3_program);

			pio_controllers_init(pio_, offset0, offset1, 0, 100.0);
		}

		std::array<controller_state, 4> trigger()
		{
			pio_sm_put(pio_, 0, 0);
			vTaskDelay(1); // FIXME is there a better way to block
						   // intelligently?

			std::array<controller_state, 4> result;
			for (size_t i = 0; i < 4; ++i)
			{
				uint32_t data = pio_sm_get(pio_, i);
				// Order:
				// B Y SELECT START UP DOWN LEFT RIGHT A X L R ^ ^ ^ ^
				result[i] = controller_state {
					.connected =
						((data >> 24) & 1) &&
						((data >> 26) & 1) &&
						((data >> 28) & 1) &&
						((data >> 30) & 1)
					,
					.x = static_cast<int8_t>(
						(data & (1 << 12)) ? -127 :
						(data & (1 << 14)) ? 127 : 0
					),
					.y = static_cast<int8_t>(
						(data & (1 << 8 )) ? 127 :
						(data & (1 << 10)) ? -127 : 0
					),
					.buttons = static_cast<uint8_t>(
						((data >> 0) & 1) |
						((data >> 2) & 1) << 1 |
						((data >> 3) & 1) << 2 |
						((data >> 4) & 1) << 3 |
						((data >> 16) & 1) << 4 |
						((data >> 18) & 1) << 5 |
						((data >> 20) & 1) << 6|
						((data >> 22) & 1) << 7
					),
				};
			}

			return result;
		}
	private:
		PIO pio_;
	};
}

#endif//SCTU_PIO_CONTROLLER_H_
