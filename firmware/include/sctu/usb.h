// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_USB_H_
#define SCTU_USB_H_

#include <cstdint>

/** Returns a bitmask describing the number of configured USB HID controllers.
 *
 * It is safe to call this from threads other than the USB one.
 *
 * bit 0 -> Player 1
 * bit 1 -> Player 2
 * bit 2 -> Player 3
 * bit 3 -> Player 4
 *
 * @returns A bitmask describing the number of configured controllers.
 */
uint8_t usb_get_active_controllers();

/** Enables the specified controller's HID interface.
 *
 * @param[in] The index of the controller to initialize, starting at 0 (e.g.
 *  P1 == 0, P2 == 1, etc.).
 */
void usb_enable_controller(uint8_t controller);

/** Disables the specified controller's HID interface.
 *
 * @param[in] The index of the controller to disable, starting at 0 (e.g.
 *  P1 == 0, P2 == 1, etc.).
 */
void usb_disable_controller(uint8_t controller);

#endif//SCTU_USB_H_
