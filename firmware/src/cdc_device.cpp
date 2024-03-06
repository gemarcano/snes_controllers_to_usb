// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <sctu/cdc_device.h>

#include <tusb.h>

#include <span>

#include <errno.h>
#undef errno
extern int errno;

namespace sctu
{
	bool cdc_device::open()
	{
		while(!connected_);
		return true;
	}

	bool cdc_device::close()
	{
		return true;
	}

	void cdc_device::update()
	{
		connected_ = tud_cdc_connected();
	}

	int cdc_device::write(std::span<const unsigned char> data)
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

	int cdc_device::read(std::span<unsigned char> buffer)
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

	cdc_device cdc;
}

