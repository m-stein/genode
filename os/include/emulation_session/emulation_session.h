/*
 * \brief   Emulation session interface
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATION_SESSION__EMULATION_SESSION_H_
#define _INCLUDE__EMULATION_SESSION__EMULATION_SESSION_H_

/* Genode includes */
#include <base/stdint.h>
#include <session/session.h>

namespace Emulation
{
	using namespace Genode;

	/**
	 * Emulation session interface
	 */
	struct Session : Genode::Session
	{
		/* determines affected bits within an accessed word */
		enum Access { LSB8, LSB16, LSB32 };

		/* exceptions */
		class Invalid_mmio_address : public Exception { };

		/**
		 * Name of this service
		 */
		static const char * service_name() { return "Emulation"; }

		/**
		 * Destructor
		 */
		virtual ~Session() { }

		/**
		 * Process write access that targets the emulated MMIO space
		 *
		 * \param off    MMIO offset of the targeted word
		 * \param a      affected bits within the targeted word
		 * \parem value  value that shall be written
		 */
		virtual void write_mmio(addr_t const off, Access const a,
		                        umword_t const value) = 0;

		/**
		 * Process read access that targets the emulated MMIO space
		 *
		 * \param   off  MMIO offset of the targeted word
		 * \param   a    affected bits within the targeted word
		 * \return       value that has been read
		 */
		virtual umword_t read_mmio(addr_t const off, Access const a) = 0;

		/**
		 * Register for listening to a specific emulator-local IRQ
		 *
		 * \param  irq       emulator-local address of targeted IRQ
		 * \param  irq_edge  signal for subsequent negations of the IRQ-state
		 * \return           current IRQ state
		 */
		virtual bool irq_handler(unsigned const irq,
		                         Signal_context_capability irq_edge) = 0;

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_write_mmio, void, write_mmio, addr_t, Access, umword_t);
		GENODE_RPC(Rpc_read_mmio, umword_t, read_mmio, addr_t, Access);
		GENODE_RPC(Rpc_irq_handler, bool, irq_handler,
		           unsigned, Signal_context_capability);

		GENODE_RPC_INTERFACE(Rpc_write_mmio, Rpc_read_mmio, Rpc_irq_handler);
	};
}

#endif /* _INCLUDE__EMULATION_SESSION__EMULATION_SESSION_H_ */

