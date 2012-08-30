/*
 * \brief  CPU state
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__CPU__CPU_STATE_H_
#define _INCLUDE__ARM__CPU__CPU_STATE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode {

	/**
	 * Basic CPU state
	 */
	struct Cpu_state
	{
		/**
		 * Native exception types
		 */
		enum Cpu_exception {
			RESET                  = 1,
			UNDEFINED_INSTRUCTION  = 2,
			SUPERVISOR_CALL        = 3,
			PREFETCH_ABORT         = 4,
			DATA_ABORT             = 5,
			INTERRUPT_REQUEST      = 6,
			FAST_INTERRUPT_REQUEST = 7,
			MAX_CPU_EXCEPTION      = FAST_INTERRUPT_REQUEST,
		};

		enum { MAX_GPR = 15 };

		/**********************************************************
		 ** The offset and width of any of these classmembers is **
		 ** silently expected to be this way by several assembly **
		 ** files. So take care if you attempt to change them.   **
		 **********************************************************/

		uint32_t r[MAX_GPR + 1]; /* general purpose registers */
		uint32_t cpsr;           /* current program status register */
		uint32_t cpu_exception;  /* native type of the last exception */

		/***************
		 ** Accessors **
		 ***************/

		addr_t instruction_ptr() const { return r[15]; }

		void instruction_ptr(addr_t const p) { r[15] = p; }

		addr_t return_ptr() const { return r[14]; }

		void return_ptr(addr_t const p) { r[14] = p; }

		addr_t stack_ptr() const { return r[13]; }

		void stack_ptr(addr_t const p) { r[13] = p; }

		/**
		 * Read a general purpose register
		 *
		 * \param   id  ID of the targeted register
		 * \param   v   Holds register value if this returns 1
		 */
		bool get_gpr(unsigned id, unsigned & v) const
		{
			if (id > MAX_GPR) return 0;
			v = r[id];
			return 1;
		}

		/**
		 * Override a general purpose register
		 *
		 * \param   id  ID of the targeted register
		 * \param   v   Has been written to register if this returns 1
		 */
		bool set_gpr(unsigned id, unsigned const v)
		{
			if (id > MAX_GPR) return 0;
			r[id] = v;
			return 1;
		}
	};

	/**
	 * Extend CPU state by banked registers
	 */
	struct Cpu_state_modes : Cpu_state
	{
		/**
		 * Common banked registers for exception modes
		 */
		struct Mode_state {

			enum Mode {
				UND,   /* Undefined      */
				SVC,   /* Supervisor     */
				ABORT, /* Abort          */
				IRQ,   /* Interrupt      */
				FIQ,   /* Fast Interrupt */
				MAX
			};

			addr_t spsr; /* saved program status register */
			addr_t sp;   /* banked stack pointer */
			addr_t lr;   /* banked link register */
		};

		Mode_state mode[Mode_state::MAX]; /* exception mode registers   */
		addr_t     fiq_r[5];              /* fast-interrupt mode r8-r12 */
	};
}

#endif /* _INCLUDE__ARM__CPU__CPU_STATE_H_ */
