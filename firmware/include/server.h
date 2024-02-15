// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later
// SPDX-FileCopyrightText: Gabriel Marcano, 2024
/// @file

#ifndef SCTU_SERVER_H_
#define SCTU_SERVER_H_

#include <cstdint>
#include <memory>
#include <expected>

// forward declaration of addrinfo, so we don't need to pull in the complete
// networking headers
struct addrinfo;

namespace sctu
{
	/** C++ wrapper around Berkley sockets.
	 *
	 * Objects of this class take ownership over the socket provided at
	 * construction. These objects are not copyable.
	 */
	class socket
	{
	public:
		/** Default constructor.
		 *
		 * Initializes to an invalid socket.
		 *
		 */
		socket();

		/** Constructor.
		 *
		 * Takes ownership of the provided socket.
		 *
		 * @param[in,out] sock Socket to take ownership over of.
		 */
		socket(int sock);

		/** Destructor.
		 *
		 * Shutdowns and closes the internal socket. If the object was default
		 * constructed, this does nothing.
		 */
		~socket();

		/** Shuts down the socket.
		 *
		 * Informs the networking stack to shutdown the socket in both
		 * directions, so no more input or output packets will be accepted.
		 */
		void shutdown();

		/** Closes the socket.
		 *
		 * Closes the socket and invalidates the internal handle.
		 */
		void close();

		/** Move constructor.
		 *
		 * Moves the socket owned by the parameter to this object.
		 *
		 * @param[in,out] sock Object to move internal socket from.
		 */
		socket(socket&& sock);

		/** Move assignment operator.
		 *
		 * Moves the socket owned by the parameter to this object.
		 *
		 * @param[in,out] sock Object to move internal socket from.
		 *
		 * @returns A reference to this object.
		 */
		socket& operator=(socket&& sock);

		/** Delete copy constructor.
		 *
		 * Forbid copying this object.
		 */
		socket(const socket&) = delete;

		/** Delete copy assignment operator.
		 *
		 * Forbid copying this object.
		 */
		socket& operator=(const socket&) = delete;

		/** Gets the raw handle of the internal socket.
		 *
		 * Note, be very careful with what is done with the returned value, as
		 * it effectively is out of the control of this socket. This method is
		 * meant mostly as an escape valve for any features not exposed by this
		 * class.
		 *
		 * @returns The raw handle of the internal socket.
		 */
		int get();

	private:
		/// Internal socket handle
		int socket_;
	};

	/** Custom deleter for struct addrinfo, for use with std::unique_ptr.
	 */
	class addrinfo_deleter
	{
	public:
		/** Deletes the provided addrinfo structure.
		 *
		 * @param[in,out] info addrinfo structure to delete.
		 */
		void operator()(addrinfo *info);
	};

	using addrinfo_ptr = std::unique_ptr<addrinfo, addrinfo_deleter>;

	/** Simple server class, leveraging socket wrapper.
	 *
	 * Currently only supports ipv4.
	 */
	class server
	{
	public:
		/** Starts listening on all IP addresses associated with the default
		 * network interface at the provided port.
		 *
		 * @param[in] port Port number to listen at.
		 *
		 * @returns 0 on success, an error code otherwise.
		 */
		int listen(uint16_t port);

		/** Blocks waiting for a new incoming connection.
		 *
		 * @returns A socket on a successful connection, or an error code on a
		 *  failure.
		 */
		std::expected<socket, int> accept();

		/** FIXME this is super specialized
		 *
		 */
		static std::expected<unsigned, int> handle_request(socket sock);

		/** Closes and shuts down the server socket.
		 */
		void close();

	private:
		/// Socket used to listen
		socket socket_ipv4;
	};
}

#endif//SCTU_SERVER_H_
