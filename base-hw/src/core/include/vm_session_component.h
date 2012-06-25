/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__VM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <base/vm_state.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>
#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>

/* Core includes */
#include <dataspace_component.h>
#include <util.h>

namespace Genode {

	class Vm_session_component : public Rpc_object<Vm_session>
	{
		private:

			Dataspace_component      _ds;
			Dataspace_capability     _ds_cap;
			Allocator_guard          _alloc_guard;
			Allocator_avl            _range_alloc;
			Rpc_entrypoint          *_ds_ep;

		public:

			Vm_session_component(Allocator      *allocator,
			                     size_t          amount,
			                     Rpc_entrypoint *ds_ep)
			: _alloc_guard(allocator, amount),
			  _range_alloc(&_alloc_guard),
			  _ds_ep(ds_ep)
			{
				addr_t ds_addr = 0;

				/* align needed dataspace size to page-size */
				size_t ds_size = align_addr(sizeof(Vm_state), get_page_size_log2());

				/* alloc needed memory */
				if (!_range_alloc.alloc_aligned(ds_size, (void**)&ds_addr,
				                                get_page_size_log2()))
					throw Root::Quota_exceeded();

				/* construct dataspace object */
				_ds = Dataspace_component(ds_size, ds_addr, true);

				/* make dataspace object known to entrypoint */
				_ds_cap = static_cap_cast<Dataspace>(_ds_ep->manage(&_ds));
			}

			~Vm_session_component()
			{
				/* dissolve VM dataspace from service entry point */
				_ds_ep->dissolve(&_ds);

				/* free region in allocator */
				_range_alloc.free((void*)_ds.core_local_addr());
			}


			/**************************
			 ** Vm session interface **
			 **************************/

			Dataspace_capability dataspace() { return _ds_cap; }

			void start() {
				Kernel::switch_to_vm((void*)_ds.core_local_addr()); }
	};
}

#endif /* _CORE__INCLUDE__VM_SESSION_COMPONENT_H_ */
