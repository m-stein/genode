/*
 * \brief  VM root interface
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__VM_ROOT_H_
#define _CORE__INCLUDE__VM_ROOT_H_

#include <root/component.h>

#include "vm_session_component.h"

namespace Genode {

	class Vm_root : public Root_component<Vm_session_component>
	{

		private:

			Range_allocator *_ram_alloc;
			Range_allocator *_io_alloc;
			Rpc_entrypoint  *_ds_ep;

		protected:

			Vm_session_component *_create_session(const char *args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").long_value(0);

				if (ram_quota < sizeof(Vm_session_component))
					throw Quota_exceeded();

				return new (md_alloc())
					Vm_session_component(_ram_alloc, _io_alloc,
					                     ram_quota - sizeof(Vm_session_component),
					                     _ds_ep);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep    entry point for managing vm session objects
			 * \param ds_ep         entry point for managing dataspaces
			 * \param ram_alloc     ram allocator
			 * \param ram_alloc     io memory allocator
			 * \param md_alloc      meta-data allocator to be used by root component
			 */
			Vm_root(Rpc_entrypoint  *session_ep,
			        Rpc_entrypoint  *ds_ep,
			        Range_allocator *ram_alloc,
			        Range_allocator *io_alloc,
			        Allocator       *md_alloc)
			: Root_component<Vm_session_component>(session_ep, md_alloc),
			  _ram_alloc(ram_alloc),
			  _io_alloc(io_alloc),
			  _ds_ep(ds_ep) { }
	};
}

#endif /* _CORE__INCLUDE__VM_ROOT_H_ */
