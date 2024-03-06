// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_CONTROLLER_H_
#define SCTU_CONTROLLER_H_

#include <cstdint>

namespace sctu
{
	/** Represents SNES controller state.
	 */
	struct controller
	{
		bool connected;
		int8_t x;        // x axis, [-127, 127]
		int8_t y;        // y axis, [-127, 127]
		uint8_t buttons; // buttons, 8 buttons
		bool operator==(const controller&) const = default;
	};
}

#endif//SCTU_CONTROLLER_H_
