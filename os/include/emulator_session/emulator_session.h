/*
 * \brief   Emulator session interface
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATOR_SESSION__EMULATOR_SESSION_H_
#define _INCLUDE__EMULATOR_SESSION__EMULATOR_SESSION_H_

/* Genode includes */
#include <base/stdint.h>
#include <session/session.h>

namespace Emulator
{
	/**
	 * Emulator session interface
	 */
	struct Session : Genode::Session
	{
		/* import Genode types */
		typedef Genode::addr_t addr_t;
		typedef Genode::umword_t umword_t;
		typedef Genode::Exception Exception;

		/* determines affected bits within an accessed word */
		enum Access { LSB8, LSB16, LSB32 };

		/* exceptions */
		class Invalid_mmio_address : public Exception { };

		/**
		 * Name of this service
		 */
		static const char * service_name() { return "Emulator"; }

		/**
		 * Destructor
		 */
		virtual ~Session() { }

		/**
		 * Process write access that targets the emulated MMIO space
		 *
		 * \param   off    MMIO offset of the targeted word 
		 * \param   a      affected bits within the targeted word
		 * \parem   value  value that shall be written
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

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_write_mmio, void, write_mmio, addr_t, Access, umword_t);
		GENODE_RPC(Rpc_read_mmio, umword_t, read_mmio, addr_t, Access);

		GENODE_RPC_INTERFACE(Rpc_write_mmio, Rpc_read_mmio);
	};
}

#endif /* _INCLUDE__EMULATOR_SESSION__EMULATOR_SESSION_H_ */

