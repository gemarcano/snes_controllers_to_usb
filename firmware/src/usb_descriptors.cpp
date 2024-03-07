/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org),
 * Copyright (c) 2024 Gabriel Marcano (gabemarcano@yahoo.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <tusb.h>

#include <pico/unique_id.h>

#include <FreeRTOS.h>
#include <task.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <concepts>
#include <vector>
#include <atomic>
#include <span>
#include <memory>
#include <bit>

// Gamepad Report Descriptor Template
// with 8 buttons, 1 joysticks
// | X | Y (1 byte each) | Button Map (1 byte) |
#define SNES_HID_REPORT_DESC_GAMEPAD(...) \
	HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
	HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  )                 ,\
	HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
		/* Report ID if any */\
		__VA_ARGS__ \
		/* 8 bit X, Y, (min -127, max 127 ) */ \
		HID_USAGE_PAGE     ( HID_USAGE_PAGE_DESKTOP                 ) ,\
		HID_USAGE          ( HID_USAGE_DESKTOP_X                    ) ,\
		HID_USAGE          ( HID_USAGE_DESKTOP_Y                    ) ,\
		HID_LOGICAL_MIN    ( 0x81                                   ) ,\
		HID_LOGICAL_MAX    ( 0x7f                                   ) ,\
		HID_REPORT_COUNT   ( 2                                      ) ,\
		HID_REPORT_SIZE    ( 8                                      ) ,\
		HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		/* 8 bit Button Map */ \
		HID_USAGE_PAGE     ( HID_USAGE_PAGE_BUTTON                  ) ,\
		HID_USAGE_MIN      ( 1                                      ) ,\
		HID_USAGE_MAX      ( 8                                      ) ,\
		HID_LOGICAL_MIN    ( 0                                      ) ,\
		HID_LOGICAL_MAX    ( 1                                      ) ,\
		HID_REPORT_COUNT   ( 8                                      ) ,\
		HID_REPORT_SIZE    ( 1                                      ) ,\
		HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
	HID_COLLECTION_END \

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
		                       _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
const tusb_desc_device_t desc_device =
{
		.bLength            = sizeof(tusb_desc_device_t),
		.bDescriptorType    = TUSB_DESC_DEVICE,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = TUSB_CLASS_MISC,
		.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
		.bDeviceProtocol    = MISC_PROTOCOL_IAD,
		.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

		.idVendor           = 0x6666,
		.idProduct          = USB_PID,
		.bcdDevice          = 0x0100,

		.iManufacturer      = 0x01,
		.iProduct           = 0x02,
		.iSerialNumber      = 0x03,

		.bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
const uint8_t* tud_descriptor_device_cb(void)
{
	return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

const auto desc_hid_report = std::to_array<uint8_t>
({
	SNES_HID_REPORT_DESC_GAMEPAD()
});

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
const uint8_t* tud_hid_descriptor_report_cb(uint8_t itf)
{
	// All HID descriptors have the same report, so we don't need to select
	// between them
	(void) itf;
	return desc_hid_report.data();
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

constexpr unsigned config_total_length(unsigned hid)
{
	return TUD_CONFIG_DESC_LEN + (hid * TUD_HID_DESC_LEN) + TUD_CDC_DESC_LEN;
}

// We'll rebuild the descriptor manually, as the number of controllers plugged
// in varies. The first two interfaces are for CDC, and then there are 0 to 4
// controllers plugged in.
static std::vector<uint8_t> desc_configuration;

enum
{
	ITF_NUM_CDC,
	ITF_NUM_CDC_DATA,
	ITF_NUM_HID_BASE, // Controller 1 is +0, Controller 2 is +1, etc.
};

const constexpr int EPNUM_CDC_NOTIF = 0x81;
const constexpr int EPNUM_CDC_OUT   = 0x02;
const constexpr int EPNUM_CDC_IN    = 0x82;
const constexpr int EPNUM_HID_BASE  = 0x03;

void update_configuration(uint8_t controller_bitmask)
{
	// Clamp the maximum number of controllers to the maximum the USB library
	// can manage for HID devices.
	controller_bitmask &= ((1u << CFG_TUD_HID) - 1);

	const unsigned number_of_controllers = std::popcount(controller_bitmask);
	const uint8_t total_interfaces = static_cast<uint8_t>(2 + number_of_controllers);

	// Update the configuration descriptor. The first two interfaces are always
	// the CDC control and data ones. If there are any controllers, they
	// follow the CDC descriptors.
	desc_configuration = std::vector<uint8_t> {
		TUD_CONFIG_DESCRIPTOR(
			1,                                          // config number
			total_interfaces,                           // interface count
			0,                                          // string index
			config_total_length(number_of_controllers), // total length
			0x00,                                       // attribute
			500                                         // power in mA
		),

		TUD_CDC_DESCRIPTOR(
			ITF_NUM_CDC,     // interface number
			4,               // string index
			EPNUM_CDC_NOTIF, // ep notification address
			8,               // ep notification size
			EPNUM_CDC_OUT,   // ep data address out
			EPNUM_CDC_IN,    // ep data address in
			64               // size
		),
	};

	// Correctly attribute the string index to the interface based on the
	// actual controller bit enabled
	// To do that, we need to track both the controller index (i), and the
	// number of the interfaces that need to be configured (count).
	for (uint8_t i = 0, count = 0; i < 4; ++i)
	{
		// Skip if the controller is not enabled
		if (!((controller_bitmask >> i) & 1))
			continue;

		const uint8_t itf_num = static_cast<uint8_t>(ITF_NUM_HID_BASE + count);
		const uint8_t ep_addr = static_cast<uint8_t>(0x80 | (EPNUM_HID_BASE + count));
		auto hid = std::to_array<uint8_t>({
			TUD_HID_DESCRIPTOR(
				itf_num,                     // interface number
				static_cast<uint8_t>(5 + i), // string index
				HID_ITF_PROTOCOL_NONE,       // protocol
				sizeof(desc_hid_report),     // report descriptor length
				ep_addr,                     // ep in & out address
				CFG_TUD_HID_EP_BUFSIZE,      // size
				10                           // polling interval
			),
		});
		desc_configuration.insert(
			std::end(desc_configuration), std::begin(hid), std::end(hid));
		++count;
	}
}

// Make this atomic so we can read it from any thread/task
static std::atomic<uint8_t> active_controllers = 0;

uint8_t usb_get_active_controllers()
{
	return active_controllers;
}

void usb_enable_controller(uint8_t controller)
{
	uint8_t active = active_controllers;
	controller &= 0xF; // Just 4 controllers
	if (controller & active)
		return;
	active |= controller;
	active_controllers = active;

	tud_disconnect();
	// FIXME is there a better way to know when we're disconnected?
	vTaskDelay(100);
	tud_connect();
}

void usb_disable_controller(uint8_t controller)
{
	uint8_t active = active_controllers;
	controller &= 0xF; // Just 4 controllers
	if ((controller & active) == 0)
		return;
	active &= ~controller;
	active_controllers = active;

	tud_disconnect();
	// FIXME is there a better way to know when we're disconnected?
	vTaskDelay(100);
	tud_connect();
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
const uint8_t* tud_descriptor_configuration_cb(uint8_t index)
{
	// we onnly have a single configuration
	(void) index; // for multiple configurations
	update_configuration(active_controllers);
	return desc_configuration.data();
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
constexpr auto string_desc_arr = std::array
{
	u"\x0409",                             // 0: is supported language is English (0x0409)
	u"Gabriel Marcano",                    // 1: Manufacturer
	u"SNES Controllers to USB Converter",  // 2: Product
	u"",                                   // 3: Serials, dynamically generated
	u"CDC",                                // 4: CDC
	u"SNES Controller P1",                 // 5: USB HID, controller 1
	u"SNES Controller P2",                 // 6: USB HID, controller 2
	u"SNES Controller P3",                 // 7: USB HID, controller 3
	u"SNES Controller P4",                 // 8: USB HID, controller 4
};

template<typename T>
concept string_like = requires(const T& s)
{
	std::basic_string_view{s};
};

template<size_t s>
constexpr size_t max_size(std::span<string_like auto, s> container)
{
	size_t max = 0;
	for (const auto& elem : container)
	{
		auto view = std::basic_string_view{elem};
		if (max < view.size())
			max = view.size();
	}
	return max;
}

static constexpr auto to_little_endian(std::integral auto value)
{
	if constexpr (std::endian::native != std::endian::little)
	{
		return std::byteswap(value);
	}
	else
	{
		return value;
	}
}

/** Helper to get Pico ID and ready it for USB strings use.
 *
 * Making it a singleton that gets evaluated lazily as the Pico ID is
 * initialized during general construction and can run afoul of the static
 * initialization fiasco issue.
 */
class pico_id
{
public:
	static const std::array<uint16_t, PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2>& get()
	{
		if (!id_)
		{
			id_.reset(new pico_id());
		}
		return id_->data;
	}

private:
	static std::unique_ptr<pico_id> id_;

	pico_id()
	{
		pico_unique_board_id_t id;
		pico_get_unique_board_id(&id);

		// Convert ID to a hex string, don't bother using stringstream as that
		// pulls in way, way too much code.
		for (size_t i = 0; i < sizeof(id.id); ++i)
		{
			unsigned char byte = id.id[i];
			for (size_t j = 0; j < 2; ++j)
			{
				unsigned char nibble = (byte >> (4*j)) & 0xF;
				if (nibble < 10)
					nibble = '0' + nibble;
				else
					nibble = 'A' + (nibble - 10);
				data[2*i + j] = to_little_endian(static_cast<uint16_t>(nibble));
			}
		}
	}

	std::array<uint16_t, PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2> data;
};
std::unique_ptr<pico_id> pico_id::id_{nullptr};

// Maximum USB string buffer size in 16 bit units
constexpr size_t desc_max =
	1 +
	std::max(
		max_size(std::span{string_desc_arr}),
		static_cast<size_t>(2*PICO_UNIQUE_BOARD_ID_SIZE_BYTES)
	)
;

// Long-lived buffer containing string to be sent over USB hardware.
static std::array<uint16_t, desc_max> _desc_str;

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	// We only support enlgish, and I'm just going to send all text regardless
	// of language id
	(void) langid;

	uint8_t chr_count = 0;
	if (index == 3)
	{
		static_assert(sizeof(pico_id::get()) < (sizeof(_desc_str) - 1));
		memcpy(_desc_str.data() + 1, pico_id::get().data(), pico_id::get().size() * 2);
		chr_count = pico_id::get().size();
	}
	else
	{
		// Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
		// https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

		if (index > string_desc_arr.size())
			return nullptr;

		const std::u16string_view str = string_desc_arr[index];

		chr_count = str.length();

		for(uint8_t i = 0; i < chr_count; ++i)
		{
			_desc_str[1+i] = to_little_endian(static_cast<uint16_t>(str[i]));
		}
	}

	// first byte is length (including header), second byte is string type
	_desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

	return _desc_str.data();
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
	// TODO not Implemented
	(void) itf;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) reqlen;

	return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
	// This example doesn't use multiple report and report ID
	(void) itf;
	(void) report_id;
	(void) report_type;
	(void)buffer;
	(void)bufsize;
}
