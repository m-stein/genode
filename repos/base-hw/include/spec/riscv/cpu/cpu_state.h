/**
 * \brief  CPU state
 * \author Sebastian Sumpf
 * \date   2015-06-01
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RISCV__CPU__CPU_STATE_H_
#define _INCLUDE__RISCV__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {
	struct Cpu_state;
}

struct Genode::Cpu_state
{
	enum Cpu_exception {
		INSTRUCTION_UNALIGNED  = 0,
		INSTRUCTION_PAGE_FAULT = 1,
		INSTRUCTION_ILLEGAL    = 2,
		LOAD_UNALIGNED         = 4,
		LOAD_PAGE_FAULT        = 5,
		STORE_UNALIGNED        = 6,
		STORE_PAGE_FAULT       = 7,
		SUPERVISOR_CALL        = 8,
		RESET                  = 16,
		IRQ_FLAG               = 1UL << 63,
	};

	addr_t ip;
	addr_t cpu_exception;
	addr_t ra;
	addr_t sp;
	addr_t gp;
	addr_t tp;
	addr_t t0;
	addr_t t1;
	addr_t t2;
	addr_t s0;
	addr_t s1;
	addr_t a0;
	addr_t a1;
	addr_t a2;
	addr_t a3;
	addr_t a4;
	addr_t a5;
	addr_t a6;
	addr_t a7;
	addr_t s2;
	addr_t s3;
	addr_t s4;
	addr_t s5;
	addr_t s6;
	addr_t s7;
	addr_t s8;
	addr_t s9;
	addr_t s10;
	addr_t s11;
	addr_t t3;
	addr_t t4;
	addr_t t5;
	addr_t t6;

	bool      is_irq() { return cpu_exception & IRQ_FLAG; }
	unsigned  irq()    { return cpu_exception ^ IRQ_FLAG; }
};

#endif /* _INCLUDE__RISCV__CPU__CPU_STATE_H_ */
