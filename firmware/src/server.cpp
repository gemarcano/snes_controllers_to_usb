// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#include <server.h>
#include <log.h>

#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include <string>

namespace sctu
{
	socket::socket()
	:socket_(-1)
	{}

	socket::socket(int sock)
	:socket_(sock)
	{}

	socket::~socket()
	{
		if (socket_ != -1)
		{
			sys_log.push(std::format("closing socket {}", socket_));
			shutdown();
			close();
		}
	}

	void socket::shutdown()
	{
		::shutdown(socket_, SHUT_RDWR);
	}

	void socket::close()
	{
		::close(socket_);
		socket_ = -1;
	}

	socket::socket(socket&& sock)
	:socket_(-1)
	{
		sys_log.push(std::format("swap socket {} {}", sock.socket_, socket_));
		std::swap(socket_, sock.socket_);
	}

	socket& socket::operator=(socket&& sock)
	{
		std::swap(socket_, sock.socket_);
		return *this;
	}

	int socket::get()
	{
		return socket_;
	}

	void addrinfo_deleter::operator()(addrinfo *info)
	{
		if (info)
			freeaddrinfo(info);
	}

	int server::listen(uint16_t port)
	{
		// Look up IP address of NTP server first, in case we're looking at an
		// NTP pool
		const addrinfo hints = {
			.ai_flags = 0,
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = 0,
			.ai_addrlen = 0,
			.ai_addr = 0,
			.ai_canonname = 0,
			.ai_next = 0,
		};
		addrinfo_ptr result = NULL;
		addrinfo *result_ = NULL;
		int err = getaddrinfo("0.0.0.0", std::to_string(port).c_str(), &hints, &result_);
		if (err == -1)
			return errno;
		result.reset(result_);
		result_ = NULL;

		socket sock{::socket(result->ai_family, result->ai_socktype, result->ai_protocol)};
		if (sock.get() == -1)
			return errno;

		err = bind(sock.get(), result->ai_addr, result->ai_addrlen);
		if (err == -1)
			return errno;

		// FIXME Should we only have a queue depth of 1?
		err = ::listen(sock.get(), 1);
		if (err == -1)
			return errno;

		socket_ipv4 = std::move(sock);

		return 0;
	}

	std::expected<socket, int> server::accept()
	{
		struct sockaddr_storage remote_addr;
		socklen_t addr_size = sizeof(remote_addr);
		int sock = ::accept(socket_ipv4.get(), reinterpret_cast<sockaddr*>(&remote_addr), &addr_size);
		if (sock == -1)
			return std::unexpected(errno);
		return socket(sock);
	}

	std::expected<unsigned, int> server::handle_request(socket sock)
	{
		unsigned result = 0;
		char buffer[16] = {};
		ssize_t amount = recv(sock.get(), buffer, 4, 0);
		if (amount != 4)
		{
			return std::unexpected(static_cast<int>(result));
		}
		memcpy(&result, buffer, sizeof(result));
		result = ntohl(result);

		return result;
	}

	void server::close()
	{
		socket_ipv4.shutdown();
		socket_ipv4.close();
	}
}
