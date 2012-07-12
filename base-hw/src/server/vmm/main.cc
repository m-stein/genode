/*
 * \brief  Virtual Machine Monitor
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/elf.h>
#include <base/env.h>
#include <base/exception.h>
#include <base/sleep.h>
#include <base/vm_state.h>
#include <io_mem_session/connection.h>
#include <rom_session/connection.h>
#include <vm_session/connection.h>
#include <os/config.h>

/* local includes */
#include "tsc_380.h"
#include "bp_147.h"

namespace Genode {

	class Vm {

		private:

			class Region : public List<Region>::Element
			{
				private:

					char              _name[64];
					addr_t            _phys_addr;
					size_t            _sz;
					Io_mem_connection _io_mem;
					void             *_local_addr;

				public:

					Region(const char* name, addr_t addr, size_t sz)
					: _phys_addr(addr),
					  _sz(sz),
					  _io_mem(addr, sz),
					  _local_addr(env()->rm_session()->attach(_io_mem.dataspace()))
					{
						strncpy(_name, name, sizeof(_name));
					}

					~Region() { env()->rm_session()->detach(_local_addr); }

					bool fits(addr_t addr, size_t sz) {
						return (addr >= _phys_addr &&
						        (addr+sz) <= (_phys_addr+_sz)); }

					void* offset(addr_t addr) {
						return (void*)((addr - _phys_addr) + (addr_t)_local_addr); }

					void dump() {
						printf("     %s:\n", _name);
						printf("         phys region  %08lx-%08lx\n",
						       _phys_addr, _phys_addr+_sz);
						printf("         local region %08lx-%08lx\n",
						       (addr_t)_local_addr, ((addr_t)_local_addr)+_sz);

//						if (strcmp("mainmem", _name, sizeof(_name)) == 0) {
//						for (unsigned i = 0; i < 300; i++) {
//							printf("%08lx ", _phys_addr+i*0x10);
//							for (unsigned j = 0; j < 4; j++) {
//								char *ptr = (char*)((addr_t)_local_addr+i*0x10+j*4);
//								for (unsigned k = 0; k < 4; k++)
//									printf("%02x", ptr[k]);
//								printf(" ");
//							}
//							printf("\n");
//						}
//						}
					}
			};


			Vm_connection  _vm_con;
			Rom_connection _rom;
			Vm_state      *_state;
			List<Region>   _regions;
			Tsc_380       *_tsc;
			Bp_147        *_tpc;

			void _load_elf()
			{
				/* attach ELF locally */
				addr_t elf_addr = env()->rm_session()->attach(_rom.dataspace());

				/* setup ELF object and read program entry pointer */
				Elf_binary elf((addr_t)elf_addr);
				_state->pc = elf.entry();
				if (!elf.valid())
					throw Invalid_elf();

				Elf_segment seg;
				for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
					if (seg.flags().skip) continue;

					addr_t addr  = (addr_t)seg.start();
					size_t size  = seg.mem_size();

					Region* region = _regions.first();
					for (; region; region = region->next())
						if (region->fits(addr, size))
							break;

					if (!region)
						throw Invalid_elf(); //TODO throw something different

					void  *base  = region->offset(addr);
					addr_t laddr = elf_addr + seg.file_offset();

					/* copy contents */
					memcpy(base, (void *)laddr, seg.file_size());

					/* if writeable region potentially fill with zeros */
					if (size > seg.file_size() && seg.flags().w)
						memset((void *)((addr_t)base + seg.file_size()),
						       0, size - seg.file_size());
				}

				/* detach ELF */
				env()->rm_session()->detach((void*)elf_addr);
			}

		public:

			class Invalid_elf : Exception {};


			Vm(const char *image, Tsc_380 *tsc, Bp_147 *tpc)
			: _rom(image),
			  _state((Vm_state*)env()->rm_session()->attach(_vm_con.dataspace())),
			  _tsc(tsc), _tpc(tpc)
			{
				memset((void*)_state, 0, sizeof(Vm_state));
				_state->cpsr = 0x1d3;
			}

			void add_region(const char *name, addr_t addr, size_t sz)
			{
				//TODO program address space protection controller

				/* converts ram region into io-memory */
				_vm_con.add_region(addr, sz);

				/* attach region */
				_regions.insert(new (env()->heap()) Region(name, addr, sz));
			}

			void add_irq(const char *name, size_t nr)
			{
				//TODO program irq controller
			}

			void start()
			{
				_load_elf();
				dump_regions();
			}

			void run() { _vm_con.start(); }

			void dump_regions()
			{
				printf("Region dump:\n");
				Region* region = _regions.first();
				for (; region; region = region->next())
					region->dump();
				printf("\n");
			}

			void dump()
			{
				printf("Cpu state:\n");
				for (unsigned i = 0; i<13; i++)
					printf("  r%x       = %08lx\n", i, _state->r[i]);
				printf("  sp       = %08lx lr     = %08lx\n",
				       _state->sp_usr, _state->lr_usr);
				printf("  sp_irq   = %08lx lr_irq = %08lx\n",
				       _state->sp_irq, _state->lr_irq);
				printf("  spsr_irq = %08lx\n", _state->spsr_irq);
				printf("  sp_fiq   = %08lx lr_fiq = %08lx\n",
				       _state->sp_fiq, _state->lr_fiq);
				printf("  spsr_fiq = %08lx\n", _state->spsr_fiq);
				printf("  sp_abt   = %08lx lr_abt = %08lx\n",
				       _state->sp_abt, _state->lr_abt);
				printf("  spsr_abt = %08lx\n", _state->spsr_abt);
				printf("  sp_und   = %08lx lr_und = %08lx\n",
				       _state->sp_und, _state->lr_und);
				printf("  spsr_und = %08lx\n", _state->spsr_und);
				printf("  sp_svc   = %08lx lr_svc = %08lx\n",
				       _state->sp_svc, _state->lr_svc);
				printf("  spsr_svc = %08lx\n", _state->spsr_svc);
				printf("  pc = %08lx cpsr = %08lx\n", _state->pc, _state->cpsr);
				printf("  exit reason = %lx\n", _state->exit_reason);
			}

			Vm_state *state() const { return _state; }
	};
}


static void parse_regions(Genode::Xml_node node, Genode::Vm *vm)
{
	using namespace Genode;

	char name[64];
	addr_t addr = 0;
	size_t size = 0;

	for (size_t j = 0; j < node.num_sub_nodes(); j++) {
		node.sub_node(j).attribute("name").value(name, sizeof(name));
		node.sub_node(j).attribute("start").value(&addr);
		node.sub_node(j).attribute("size").value(&size);
		vm->add_region(name, addr, size);
	}
}


static void parse_irqs(Genode::Xml_node node, Genode::Vm *vm)
{
	using namespace Genode;

	char name[64];
	size_t nr = 0;
	for (size_t j = 0; j < node.num_sub_nodes(); j++) {
		node.sub_node(j).attribute("name").value(name, sizeof(name));
		node.sub_node(j).attribute("number").value(&nr);
		vm->add_irq(name, nr);
	}
}


enum {
	TPC_VEA9X4_BASE = 0x100e6000,
	TSC_VEA9X4_BASE = 0x100ec000,
};


int main() {

	static Genode::Io_mem_connection tsc_io_mem(TSC_VEA9X4_BASE, 0x1000);
	static Genode::Io_mem_connection tpc_io_mem(TPC_VEA9X4_BASE, 0x1000);
	static Genode::Tsc_380
		tsc((Genode::addr_t)Genode::env()->rm_session()->attach(tsc_io_mem.dataspace()));
	static Genode::Bp_147
		tpc((Genode::addr_t)Genode::env()->rm_session()->attach(tpc_io_mem.dataspace()));

	try {
		char name[64];
		Genode::Xml_node config = Genode::config()->xml_node();

		/* read image file to load, and construct Vm object */
		config.attribute("image").value(name, sizeof(name));
		Genode::Vm vm(name, &tsc, &tpc);

		/* parse memory regions and irqs used by virtual-machine */
		for (Genode::size_t i = 0; i < config.num_sub_nodes(); i++) {
			if (config.sub_node(i).has_type("memory"))
				parse_regions(config.sub_node(i), &vm);
			else
				if (config.sub_node(i).has_type("interrupts"))
				parse_irqs(config.sub_node(i), &vm);
		}

		vm.start();
		while (true) {
			vm.run();
			Genode::printf("exited due to %d\n", vm.state()->exit_reason);
//			vm.dump();
		}
	} catch (Genode::Rm_session::Attach_failed) {
		PERR("Rm_session::Attach_failed failed");
		return -1;
	} catch (Genode::Vm::Invalid_elf) {
		PERR("Invalid elf image");
		return -2;
	} catch (Genode::Vm_session::Region_conflict) {
		PERR("Region conflict");
		return -3;
	} catch (...) {
		PERR("Configuration error.");
		return -4;
}
	//Genode::sleep_forever();
	return 0;
}
