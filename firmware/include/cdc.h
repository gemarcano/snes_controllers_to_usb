// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef CDC_H_
#define CDC_H_

#include <cstdint>
#include <atomic>
#include <array>
#include <span>

#include <tusb.h>

#include <errno.h>
#undef errno
extern int errno;

namespace sctu
{
	/**Abstract class representing IO devices.
	 */
	class io_device
	{
	public:
		virtual ~io_device() = default;
		/** Open the device.
		 *
		 * This is meant to initialize the hardware or anything else required
		 * to enable use of write and read. This may block until the device is
		 * ready.
		 *
		 * @returns True on success, false otherwise.
		 */
		virtual bool open() = 0;

		/** Close the device.
		 *
		 * This frees up any resources taken during open.
		 *
		 * @returns True on success, false otherwise.
		 */
		virtual bool close() = 0;

		/** Writes the span of data to the device.
		 *
		 * The actual number of bytes written may be less than the number
		 * requested.
		 *
		 * @param[in] data Span of data to write to the device.
		 *
		 * @returns The actual number of bytes written, or -1 on an error
		 *  (errno should also be set to something sensible).
		 */
		virtual int write(std::span<const unsigned char> data) = 0;

		/** Reads data from the device.
		 *
		 * The actual number of bytes read may be less than the size of the
		 * span, due to EOF or the device not having any more data at that
		 * time.
		 *
		 * @param[in] data Span of data to receive data from the device.
		 *
		 * @returns The actual number of bytes read, or -1 on an error
		 *  (errno should also be set to something sensible).
		 */
		virtual int read(std::span<unsigned char> buffer) = 0;
	};

	/** IO device representing a TinyUSB CDC serial connection.
	 */
	class cdc_device : public io_device
	{
	public:
		bool open() override
		{
			while(!connected_);
			return true;
		}

		bool close() override
		{
			return true;
		}

		/** Updates the connected status of the TinyUSB CDC device.
		 *
		 * This can be called from multiple threads.
		 */
		void update()
		{
			connected_ = tud_cdc_connected();
		}

		int write(std::span<const unsigned char> data) override
		{
			if (!connected_)
			{
				errno = ENXIO;
				return -1;
			}

			if (tud_cdc_write_available() == 0)
			{
				tud_task();
				tud_cdc_write_flush();
			}

			uint32_t result = tud_cdc_write(data.data(), data.size());
			tud_cdc_write_flush();
			return static_cast<int>(result);
		}

		int read(std::span<unsigned char> buffer) override
		{
			if (!connected_)
			{
				errno = ENXIO;
				return -1;
			}

			while(!tud_cdc_available())
			{
				// FIXME some kind of yield?
			}

			// There is data available
			size_t read_;
			for (read_ = 0; read_ < buffer.size() && tud_cdc_available();)
			{
				// read and echo back
				read_ += tud_cdc_read(buffer.data() + read_, buffer.size() - read_);
			}
			return static_cast<int>(read_);
		}

	private:
		/// Atomic flag indicating whether the USB CDC device is connected.
		std::atomic_bool connected_;
	};

	extern cdc_device cdc;
}

#endif//CDC_H_
