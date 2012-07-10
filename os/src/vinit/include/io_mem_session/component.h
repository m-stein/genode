/*
 * \brief  Session component of an emulated IO_MEM service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IO_MEM_SESSION__COMPONENT_H_
#define _INCLUDE__IO_MEM_SESSION__COMPONENT_H_

/* Genode includes */
#include <base/thread.h>
#include <io_mem_session/io_mem_session.h>

/* local includes */
#include <rm_session/connection.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Pagefault handler for emulated IO_MEM regions
	 */
	class Io_mem_fault_handler : public Thread<8*1024>
	{
		Rm_session * const _rm; /* nested RM wich backs the IO_MEM dataspace */
		Signal_receiver * const _sig_recvr; /* receives fault signals */
		Emulation::Session * const _emu; /* session to emulate
		                                  * sideeffects of the faults */
		addr_t const _io_mem_base; /* emulator-local base of the IO region */

		public:

			/**
			 * Constructor
			 */
			Io_mem_fault_handler(Rm_session * const rm,
			                     Signal_receiver * const sr,
			                     Emulation::Session * const emu,
			                     addr_t const io_mem_base)
			:
				_rm(rm), _sig_recvr(sr), _emu(emu), _io_mem_base(io_mem_base)
			{ }

			/**
			 * Process one pending fault
			 */
			void handle_fault()
			{
				/* fetch fault attributes */
				using namespace Genode;
				Rm_session::State s = _rm->state();
				if (s.type == Rm_session::READY) {
					PWRN("Ignoring spurious fault signal");
					return;
				}
				/* emulate instruction through our emulator */
				switch(s.type)
				{
				case Rm_session::WRITE_FAULT: {
					_emu->write_mmio(_io_mem_base + s.addr,
					                 Emulation::Session::LSB32, s.value);
					break; }
				case Rm_session::READ_FAULT: {
					s.value = _emu->read_mmio(_io_mem_base + s.addr,
					                          Emulation::Session::LSB32);
					break; }
				default: return;
				}
				/* end fault */
				_rm->processed(s);
			}

			/**
			 * Thread main routine
			 */
			void entry()
			{
				while (1) {
					Signal s = _sig_recvr->wait_for_signal();
					for (int i = 0; i < s.num(); i++) handle_fault();
				}
			}
	};

	/**
	 * Session component of an emulated IO_MEM service
	 */
	class Io_mem_session_component : public Rpc_object<Io_mem_session>
	{
		Rm_connection _rm;
		Signal_receiver _fault_recvr;
		Signal_context _fault;
		Io_mem_fault_handler _fault_handler;

		public:

			/**
			 * Constructor
			 */
			Io_mem_session_component(Rm_root * const rm_root,
			                         addr_t const base,
			                         size_t const size,
			                         Emulation::Session * const emulation)
			:
				_rm(rm_root, 0, size),
				_fault_handler(&_rm, &_fault_recvr, emulation, base)
			{
				/* set fault handler and start handling */
				_rm.fault_handler(_fault_recvr.manage(&_fault));
				_fault_handler.start();
			}

			/**
			 * Get emulated IO_MEM concealed as normal IO_MEM dataspace
			 */
			Io_mem_dataspace_capability dataspace()
			{
				using namespace Genode;
				return static_cap_cast<Io_mem_dataspace>(_rm.dataspace());
			}
	};
}

#endif /* _INCLUDE__IO_MEM_SESSION__COMPONENT_H_ */

