// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_USB_H_
#define SCTU_USB_H_

#include <cstdint>

/** Returns the number of configured USB HID controllers.
 *
 * It is safe to call this from threads other than the USB one.
 *
 * @returns The number of configured controllers.
 */
uint8_t get_active_controllers();

/** Updates the number of configured USB HID controllers and forces a reset of
 * the USB interface.
 *
 * This requires that some other task is executing tud_task() in a loop.
 */
void reset_configuration(uint8_t controllers);

#endif//SCTU_USB_H_
