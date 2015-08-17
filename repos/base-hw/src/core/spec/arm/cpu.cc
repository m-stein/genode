/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2015-08-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;
using namespace Arm;


static void sctlr_write(Sctlr::access_t const v) {
	asm volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r" (v) : ); }


Sctlr::access_t Arm::sctlr()
{
	Sctlr::access_t v = 0;
	asm volatile ("mrc p15, 0, %0, c1, c0, 0" :: "r" (v) : );
	return v;
}


void Arm::init_sctlr(Sctlr::access_t & v, bool const virt)
{
	Sctlr::C::set(v, 1);
	Sctlr::I::set(v, 1);
	Sctlr::V::set(v, 1);
	Sctlr::M::set(v, virt);
	sctlr_write(v);
}
