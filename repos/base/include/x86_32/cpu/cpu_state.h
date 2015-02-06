/*
 * \brief  CPU state
 * \author Christian Prochaska
 * \date   2011-04-15
 *
 * This file contains the x86_32-specific part of the CPU state.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86_32__CPU__CPU_STATE_H_
#define _INCLUDE__X86_32__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		enum Cpu_exception {
			RESET                  = 1,
			UNDEFINED_INSTRUCTION  = 2,
			SUPERVISOR_CALL        = 3,
			PREFETCH_ABORT         = 4,
			DATA_ABORT             = 5,
			INTERRUPT_REQUEST      = 6,
			FAST_INTERRUPT_REQUEST = 7,
		};

		addr_t ip;   /* instruction pointer */
		addr_t sp;   /* stack pointer       */
		addr_t edi;
		addr_t esi;
		addr_t ebp;
		addr_t ebx;
		addr_t edx;
		addr_t ecx;
		addr_t eax;
		addr_t gs;
		addr_t fs;
		addr_t eflags;
		addr_t trapno;
		addr_t cpu_exception;     /* XXX remove; last hardware exception */

		/**
		 * Constructor
		 */
		Cpu_state(): ip(0), sp(0), edi(0), esi(0), ebp(0),
		             ebx(0), edx(0), ecx(0), eax(0), gs(0),
		             fs(0), eflags(0), trapno(0) { }
	};

	struct Cpu_state_modes : Cpu_state { };
}

#endif /* _INCLUDE__X86_32__CPU__CPU_STATE_H_ */
