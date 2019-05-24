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
//extern int __exception_vectors;
//extern int _start64_bsp;

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

static inline __attribute__((always_inline)) void _outb(Genode::uint16_t portx, Genode::uint8_t val)
{
	asm volatile ("outb %b0, %w1" : : "a" (val), "Nd" (portx));
}

static inline __attribute__((always_inline)) Genode::uint8_t _inb(Genode::uint16_t port)
{
	Genode::uint8_t res;
	asm volatile ("inb %%dx, %0" :"=a"(res) :"Nd"(port));
	return res;
}

enum {

	COMPORT_DATA_OFFSET   = 0,
	COMPORT_STATUS_OFFSET = 5,

	STATUS_THR_EMPTY = 0x20,  /* transmitter hold register empty */
	STATUS_DHR_EMPTY = 0x40,  /* data hold register empty - data completely sent */

	baud_rate = 115200,
	port = 1016,
	IER  = port + 1,
	EIR  = port + 2,
	LCR  = port + 3,
	MCR  = port + 4,
	LSR  = port + 5,
	MSR  = port + 6,
	DLLO = port + 0,
	DLHI = port + 1
};

extern "C" void init()
{
//	Genode::log("exception vectors:", &__exception_vectors);
//	Genode::log("start :", &_start64_bsp);
//	for(unsigned volatile i=0; i < 1000000000; i++);
//	idt_init();
//	unsigned volatile x = *(unsigned volatile *)0;
//	Genode::log(x);
Genode::log("Hallo");


	/* wait until serial port is ready */
	Genode::uint8_t ready = STATUS_THR_EMPTY;
	while ((_inb(port + COMPORT_STATUS_OFFSET) & ready) != ready);

asm volatile(
	"movw   $0x3f8,-0x2(%rbp)\n"
	"movb   $0x41,-0x3(%rbp)\n"
	"movzbl -0x3(%rbp),%eax\n"
	"movzwl -0x2(%rbp),%edx\n"
	"out    %al,(%dx)\n");























	Bootstrap::Platform & p = Bootstrap::platform();
	p.start_core(p.enable_mmu());
}
