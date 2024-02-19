// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <log.h>
#include <syslog.h>

#include <pico/cyw43_arch.h>
#include <lwip/netdb.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <cstdint>
#include <atomic>

// This secrets.h includes strings for WIFI_SSID and WIFI_PASSWORD
#include "secrets.h"

namespace sctu
{

	static void status_callback(netif *netif_)
	{
		sys_log.push("status: changed");
		sys_log.push(std::format("status: IP Address: {}", ip4addr_ntoa(netif_ip4_addr(netif_))));
		sys_log.push(std::format("status: NETIF flags: {:#02x}", netif_->flags));
		int32_t rssi = 0;
		cyw43_wifi_get_rssi(&cyw43_state, &rssi);
		sys_log.push(std::format("status: RSSI: {}", rssi));
		sys_log.push(std::format("status: Wifi state: {}", cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA)));
	}

	static void link_callback(netif *netif_)
	{
		sys_log.push("link changed");
		sys_log.push(std::format("link: IP Address: {}", ip4addr_ntoa(netif_ip4_addr(netif_))));
		sys_log.push(std::format("link: NETIF flags: {:#02x}", netif_->flags));
		int32_t rssi = 0;
		cyw43_wifi_get_rssi(&cyw43_state, &rssi);
		sys_log.push(std::format("link: RSSI: {}", rssi));
		sys_log.push(std::format("link: Wifi state: {}", cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA)));
	}

	static void init_wifi()
	{
		sys_log.push(std::format("Connecting to SSID {}:", WIFI_SSID));
		for (;;)
		{
			int result = 0;
			if (!(result = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))) {
				sys_log.push("    DONE");
				break;
			}
			sys_log.push(std::format("    FAILED: {}", result));
		}
	}

	static std::atomic_bool wifi_initd = false;

	void wifi_management_task(void*)
	{
		sys_log.push("Initializing cyw43 with USA region...: ");
		for (;;)
		{
			// cyw43_arch_init _must_ be called within a FreeRTOS task, see
			// https://github.com/raspberrypi/pico-sdk/issues/1540
			int result = 0;
			if (!(result = cyw43_arch_init_with_country(CYW43_COUNTRY_USA)))
			{
				sys_log.push("    DONE");
				break;
			}
			sys_log.push(std::format("    FAILED: {}", result));
		}

		cyw43_arch_enable_sta_mode();
		// Turn off powersave completely
		cyw43_wifi_pm(&cyw43_state, CYW43_DEFAULT_PM & ~0xf);

		// Setup link/status callbacks
		cyw43_arch_lwip_begin();
		netif_set_status_callback(netif_default, status_callback);
		netif_set_link_callback(netif_default, link_callback);
		cyw43_arch_lwip_end();

		init_wifi();
		wifi_initd = true;

		int wifi_state = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
		TickType_t last = xTaskGetTickCount();
		for(;;)
		{
			int current_state = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
			if (current_state != CYW43_LINK_JOIN || !(netif_default->flags & NETIF_FLAG_LINK_UP))
			{
				sys_log.push(std::format("wifi: state is bad? {}", current_state));
				sys_log.push(std::format("wifi: or is it flags? {:#02x}", netif_default->flags));
				if (current_state != CYW43_LINK_DOWN)
				{
					sys_log.push(std::format("wifi: disconnecting from network"));
					cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
				}
				int connect_result;
				sys_log.push(std::format("wifi: trying to reconnect"));
				while ((connect_result = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))) {
					sys_log.push(std::format("FAILED to reconnect, result {}, trying again", connect_result));
				}
				sys_log.push(std::format("wifi: hopefully succeeded in connecting"));
				if (current_state != wifi_state)
					wifi_state = current_state;
			}
			vTaskDelayUntil(&last, 1000);
		}
	}
}
