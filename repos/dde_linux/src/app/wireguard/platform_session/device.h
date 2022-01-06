/*
 * \brief  Dummy - platform session device interface
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_SESSION__DEVICE_H_
#define _PLATFORM_SESSION__DEVICE_H_

#include <util/mmio.h>
#include <util/string.h>
#include <base/exception.h>
#include <io_mem_session/client.h>
#include <irq_session/client.h>

namespace Platform {
	struct Connection;
	class Device;

	using namespace Genode;
}


struct Platform::Connection
{
	Connection(Genode::Env &) {}
};


class Platform::Device : Interface, Noncopyable
{
	public:

		struct Range { addr_t start; size_t size; };

		struct Mmio;
		struct Irq;

		using Name = String<64>;

	public:

		struct Index { unsigned value; };

		explicit Device(Connection &) { }

		struct Type { String<64> name; };

		Device(Connection &, Type) {}
		Device(Connection &, Name) {}
		~Device() { }
};


class Platform::Device::Mmio : Range
{
	public:

		struct Index { unsigned value; };

		Mmio(Device &, Index) { }

		explicit Mmio(Device &) { }

		size_t size() const { return 0; }

		template <typename T>
		T *local_addr() { return reinterpret_cast<T *>(nullptr); }
};


class Platform::Device::Irq : Noncopyable
{
	public:

		struct Index { unsigned value; };

		Irq(Device &, Index) { }

		explicit Irq(Device &) {}

		void ack() { }
		void sigh(Signal_context_capability) { }
		void sigh_omit_initial_signal(Signal_context_capability) { }
};

#endif /* _PLATFORM_SESSION__DEVICE_H_ */
