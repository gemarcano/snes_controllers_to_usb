// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_CDC_H_
#define SCTU_CDC_H_

#include <cstdint>
#include <atomic>
#include <array>
#include <span>

#include <tusb.h>

#include <sctu/io_device.h>

namespace sctu
{
	/** IO device representing a TinyUSB CDC serial connection.
	 */
	class cdc_device : public io_device
	{
	public:
		bool open() override;

		bool close() override;

		/** Updates the connected status of the TinyUSB CDC device.
		 *
		 * This can be called from multiple threads, but it really needs to be
		 * called from the same thread/task as the main tud_task call.
		 */
		void update();

		int write(std::span<const unsigned char> data) override;

		int read(std::span<unsigned char> buffer) override;

	private:
		/// Atomic flag indicating whether the USB CDC device is connected.
		std::atomic_bool connected_ = false;
	};

	extern cdc_device cdc;
}

#endif//SCTU_CDC_H_
