// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <network_task.h>
#include <server.h>
#include <log.h>

#include <cstdint>
#include <cstdio>

namespace sctu
{
	void network_task(void*)
	{
		// Loop endlessly, restarting the server if there are errors
		sctu::server server_;

		for(;;)
		{
			// FIXME maybe move wifi initialization here?
			int err;
			do
			{
				err = server_.listen(48686);
				if (err != 0)
				{
					sys_log.push(std::format("unable to listen on server, error {}", err));
				}
			} while (err != 0);

			for(;;)
			{
				auto result = server_.accept();
				if (!result)
				{
					sys_log.push(std::format("unable to accept socket, error {}", result.error()));
					// FIXME what if the error is terminal? Are there any terminal errors?
					continue;
				}
				sys_log.push("new connection accepted");
				auto request_result = server_.handle_request(std::move(*result));
				if (result)
				{
					sys_log.push(std::format("Received network request with value {}", request_result.value()));
					//xQueueSendToBack(switch_comms.get(), &request_result.value(), 0);
				}
				else
					sys_log.push(std::format("failed to handle request: {}", result.error()));
			}

			server_.close();
		}
	}
}
