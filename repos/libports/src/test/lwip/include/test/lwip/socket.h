/*
 * \brief  Socket wrapper class
 * \author Martin Stein
 * \date   2021-01-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__LWIP__SOCKET_H_
#define _TEST__LWIP__SOCKET_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <base/log.h>

/* libc includes */
#include <sys/socket.h>
#include <unistd.h>

class Socket : private Genode::Noncopyable
{
	private:

		int const _descriptor;

	public:

		explicit Socket(int type)
		:
			_descriptor { ::socket(AF_INET, type, 0 ) }
		{ }

		~Socket()
		{
			if (descriptor_valid()) {

				if (::shutdown(_descriptor, SHUT_RDWR)) {

					Genode::error("failed to shutdown socket");
				}
				if (::close(_descriptor)) {

					Genode::error("failed to close socket");
				}
			}
		}

		bool descriptor_valid() const
		{
			return _descriptor >= 0;
		}

		int descriptor() const
		{
			if (!descriptor_valid()) {

				class Descriptor_invalid { };
				throw Descriptor_invalid { };
			}
			return _descriptor;
		}
};

#endif /* _TEST__LWIP__SOCKET_H_ */
