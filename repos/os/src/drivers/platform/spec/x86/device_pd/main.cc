/*
 * \brief  Pci device protection domain service for platform driver 
 * \author Alexander Boettcher
 * \date   2013-02-10
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>
#include <os/static_root.h>
#include <base/log.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>

#include <util/flex_iterator.h>
#include <util/retry.h>

#include <nova/native_thread.h>

#include "../pci_device_pd_ipc.h"


static bool map_eager(Genode::addr_t const page, unsigned log2_order)
{
	using Genode::addr_t;

	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());
	Nova::Rights const mapping_rw(true, true, false);

	addr_t const page_fault_portal = myself->native_thread().exc_pt_sel + 14;

	/* setup faked page fault information */
	utcb->set_msg_word(((addr_t)&utcb->qual[2] - (addr_t)utcb->msg) / sizeof(addr_t));
	utcb->ip      = reinterpret_cast<addr_t>(map_eager);
	utcb->qual[1] = page;
	utcb->crd_rcv = Nova::Mem_crd(page >> 12, log2_order - 12, mapping_rw);

	/* trigger faked page fault */
	Genode::uint8_t res = Nova::call(page_fault_portal);
	return res == Nova::NOVA_OK;
}


void Platform::Device_pd_component::attach_dma_mem(Genode::Dataspace_capability ds_cap)
{
	using namespace Genode;

	Dataspace_client ds_client(ds_cap);

	addr_t const phys = ds_client.phys_addr();
	size_t const size = ds_client.size();

	addr_t page = ~0UL;

	try {
		page = _address_space.attach_at(ds_cap, phys);
	} catch (Rm_session::Out_of_metadata) {
		throw;
	} catch (Rm_session::Region_conflict) {
		/* memory already attached before - done */
		return;
	} catch (...) { }

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			_address_space.detach(page);

		Genode::error("attachment of DMA memory @ ",
		              Genode::Hex(phys), "+", Genode::Hex(size), " failed");
		return;
	}

	Genode::Flexpage_iterator it(page, size, page, size, 0);
	for (Genode::Flexpage flex = it.page(); flex.valid(); flex = it.page()) {
		if (map_eager(flex.addr, flex.log2_order))
			continue;

		Genode::error("attachment of DMA memory @ ",
		              Genode::Hex(phys), "+", Genode::Hex(size), " failed at ",
		              flex.addr);
		return;
	}
}

void Platform::Device_pd_component::assign_pci(Genode::Io_mem_dataspace_capability io_mem_cap, Genode::uint16_t rid)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = _address_space.attach(io_mem_cap);
	/* sanity check */
	if (!page)
		throw Rm_session::Region_conflict();

	/* trigger mapping of whole memory area */
	if (!map_eager(page, 12))
		Genode::error("assignment of PCI device failed - ", Genode::Hex(page));

	/* utility to print rid value */
	struct Rid
	{
		Genode::uint16_t const v;
		explicit Rid(Genode::uint16_t rid) : v(rid) { }
		void print(Genode::Output &out) const
		{
			using Genode::print;
			using Genode::Hex;
			print(out, Hex(v >> 8, Hex::Prefix::OMIT_PREFIX), ":",
			      Hex((v >> 3) & 3, Hex::Prefix::OMIT_PREFIX), ".",
			      Hex(v & 0x7, Hex::Prefix::OMIT_PREFIX));
		}
	};

	/* try to assign pci device to this protection domain */
	if (!env()->pd_session()->assign_pci(page, rid))
		Genode::error("assignment of PCI device ", Rid(rid), " failed ",
		              "phys=", Genode::Hex(ds_client.phys_addr()), " "
		              "virt=", Genode::Hex(page));
	else
		Genode::log("assignment of ", rid, " succeeded");

	/* we don't need the mapping anymore */
	_address_space.detach(page);
}


struct Main
{
	Genode::Env &env;

	Platform::Device_pd_component pd_component { env.rm() };

	Genode::Static_root<Platform::Device_pd> root { env.ep().manage(pd_component) };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) { static Main main(env); }

