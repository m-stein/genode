/*
 * \brief  Root component of an emulated IO_MEM service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IO_MEM_SESSION__ROOT_H_
#define _INCLUDE__IO_MEM_SESSION__ROOT_H_

/* Genode includes */
#include <emulation_session/emulation_session.h>
#include <util/string.h>
#include <root/component.h>

/* local includes */
#include <io_mem_session/component.h>
#include <util/assert.h>

namespace Init
{
	using namespace Genode;

	const char * emulation_key();

	/**
	 * Root component of an emulated IO_MEM service
	 */
	class Io_mem_root : public Root_component<Io_mem_session_component>
	{
		Rm_root * const _rm_root;

		/**
		 * Create a new session for the requested MMIO
		 *
		 * \param args  Session-creation arguments
		 */
		Io_mem_session_component * _create_session(const char * args)
		{
			/* get session attributes */
			addr_t base = Arg_string::find_arg(args, "base").ulong_value(0);
			size_t size = Arg_string::find_arg(args, "size").ulong_value(0);
			Emulation::Session * const emu_session = (Emulation::Session *)
				Arg_string::find_arg(args, emulation_key()).ulong_value(0);
			assert(emu_session);

			/* create session */
			return new (md_alloc())
				Io_mem_session_component(_rm_root, base, size, emu_session);
		}

		public:

			/**
			 * Constructor
			 *
			 * \param ep        Entrypoint for the root component
			 * \param md_alloc  Meta-data allocator for the root component
			 */
			Io_mem_root(Rpc_entrypoint * const ep,
			            Allocator * const md,
			            Rm_root * const rm_root)
			:
				Root_component<Io_mem_session_component>(ep, md),
				_rm_root(rm_root)
			{ }
	};
}

#endif /* _INCLUDE__IO_MEM_SESSION__ROOT_H_ */

