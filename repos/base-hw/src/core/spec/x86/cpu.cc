/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>
#include <kernel/pd.h>

void Genode::Cpu::init_virt_kernel(Kernel::Pd * pd)
{
	/*
	 * Please do not remove the PINF(), because the serial constructor requires
	 * access to the Bios Data Area, which is available in the initial
	 * translation table set, but not in the final tables used after
	 * Cr3::write().
	 */
	PINF("Switch to core's final translation table");

	Cr3::write(Cr3::init((addr_t)pd->translation_table()));
}


void Genode::Cpu::_init_fpu()
{
	Cr0::access_t cr0_value = Cr0::read();
	Cr4::access_t cr4_value = Cr4::read();

	Cr0::Mp::set(cr0_value);
	Cr0::Em::clear(cr0_value);
	Cr0::Ts::set(cr0_value);
	Cr0::Ne::set(cr0_value);
	Cr0::write(cr0_value);

	Cr4::Osfxsr::set(cr4_value);
	Cr4::Osxmmexcpt::set(cr4_value);
	Cr4::write(cr4_value);
}


void Genode::Cpu::_disable_fpu() { Cr0::write(Cr0::read() | Cr0::Ts::bits(1)); }


bool Genode::Cpu::_fpu_enabled() { return !Cr0::Ts::get(Cr0::read()); }


Genode::Cpu::Cpu() : _fpu_state(0)
{
	if (Kernel::cpu_primary_id() == Kernel::cpu_executing_id()) {
		_idt = new (&_mt_idt) Idt();
		_idt->setup(Cpu::exception_entry);

		_tss = new (&_mt_tss) Tss();
		_tss->load();
	}
	_idt->load(Cpu::exception_entry);
	_tss->setup(Cpu::exception_entry);
}
