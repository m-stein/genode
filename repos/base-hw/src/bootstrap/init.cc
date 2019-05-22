/*
 * \brief   Initialization code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2016-09-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local */
#include <platform.h>

/* base includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>

Bootstrap::Platform & Bootstrap::platform() {
	return *unmanaged_singleton<Bootstrap::Platform>(); }

extern "C" void init() __attribute__ ((noreturn));


extern int __idt;
extern int __idt_end;

/**
 * Pseudo Descriptor
 *
 * See Intel SDM Vol. 3A, section 3.5.1
 */
struct Pseudo_descriptor
{
	Genode::uint16_t const limit = 0;
	Genode::uint64_t const base  = 0;

	constexpr Pseudo_descriptor(Genode::uint16_t l, Genode::uint64_t b)
	: limit(l), base(b) {}
} __attribute__((packed));

void idt_init()
{
	using namespace Genode;
	Pseudo_descriptor descriptor {
		(uint16_t)((addr_t)&__idt_end - (addr_t)&__idt),
		(uint64_t)(&__idt) };
	asm volatile ("lidt %0" : : "m" (descriptor));
}

extern "C" void init()
{
	idt_init();
	Bootstrap::Platform & p = Bootstrap::platform();
	p.start_core(p.enable_mmu());
}
