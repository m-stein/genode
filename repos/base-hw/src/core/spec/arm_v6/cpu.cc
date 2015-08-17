/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <cpu.h>
#include <kernel/pd.h>

using namespace Genode;

/**
 * System control register
 */
struct Sctlr : Arm::Sctlr
{
	struct W  : Bitfield<3,1>  { }; /* enable write buffer */
	struct Dt : Bitfield<16,1> { }; /* global data TCM enable */
	struct It : Bitfield<18,1> { }; /* global instruction TCM enable */
	struct U  : Bitfield<22,1> { }; /* enable unaligned data access */
	struct Xp : Bitfield<23,1> { }; /* disable subpage AP bits */
	struct Unnamed_0 : Bitfield<4,3>  { }; /* SBOP */
	struct Unnamed_1 : Bitfield<26,6> { }; /* Reserved */
};

namespace Arm
{
	void init_sctlr(Sctlr::access_t & v, bool const virt);
	Sctlr::access_t sctlr();
}


static void init_sctlr(bool const virt)
{
	Sctlr::access_t v = 0;
	Sctlr::W::set(v, 1);
	Sctlr::Dt::set(v, 1);
	Sctlr::It::set(v, 1);
	Sctlr::U::set(v, 1);
	Sctlr::Xp::set(v, 1);
	Sctlr::Unnamed_0::set(v, ~0);
	Sctlr::Unnamed_1::set(v, Sctlr::Unnamed_1::get(Arm::sctlr()));
	Arm::init_sctlr(v, virt);
}


void Cpu::init_phys_kernel()
{
	Board::prepare_kernel();
	init_sctlr(false);
	flush_tlb();

	/* check for mapping restrictions */
	assert(!restricted_page_mappings());
}


void Genode::Arm::flush_data_caches() {
	asm volatile ("mcr p15, 0, %[rd], c7, c14, 0" :: [rd]"r"(0) : ); }

void Genode::Arm::invalidate_data_caches() {
	asm volatile ("mcr p15, 0, %[rd], c7, c6, 0" :: [rd]"r"(0) : ); }


void Cpu::init_virt_kernel(Kernel::Pd* pd)
{
	Cidr::write(pd->asid);
	Dacr::write(Dacr::init_virt_kernel());
	Ttbr0::write(Ttbr0::init((Genode::addr_t)pd->translation_table()));
	Ttbcr::write(0);
	init_sctlr(true);
}
