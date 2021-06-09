/*
 * \brief  C++ back end for Ada 'Log' package
 * \author Martin Stein
 * \date   2019-12-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/ada_object.h>
#include <kernel/signal_receiver.h>
#include <kernel/ipc_node.h>
#include <kernel/cpu_scheduler.h>
#include <kernel/timer.h>
#include <pic.h>

namespace Log {

	extern "C" void print_string_with_length(char const *str,
	                                         unsigned    length)
	{
		Genode::raw(Genode::Cstring(str, length));
	}


	extern "C" void
	print_string_with_length_and_uint64(char const       *str,
	                                    unsigned          length,
	                                    Genode::uint64_t  uint64)
	{
		Genode::raw(Genode::Cstring(str, length), Genode::Hex(uint64));
	}


	extern "C" void
	print_string_with_length_and_address(char const     *str,
	                                     unsigned        length,
	                                     Genode::addr_t  addr)
	{
		Genode::raw(Genode::Cstring(str, length), Genode::Hex(addr));
	}


	extern "C" void print_uint64(Genode::uint64_t uint64)
	{
		Genode::raw(Genode::Hex(uint64));
	}


	extern "C" void print_address(Genode::addr_t addr)
	{
		Genode::raw(Genode::Hex(addr));
	}
}
