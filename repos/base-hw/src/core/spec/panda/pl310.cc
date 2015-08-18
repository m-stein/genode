/*
 * \brief  Driver for the Pl310 L2-Cache Controller
 * \author Stefan Kalkowski
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pl310.h>

Genode::Pl310::Pl310(addr_t const base) : Mmio(base)
{
	l2_cache_aux_reg(Aux::init_value());
	l2_cache_enable(true);
	write<Irq_mask>(0);
	write<Irq_clear>(0xffffffff);
}


void Genode::Pl310::flush()
{
	Debug::access_t debug = 0;
	Debug::Dwb::set(debug, 1);
	Debug::Dcl::set(debug, 1);
	l2_cache_debug_reg(debug);

	write<Clean_invalidate_by_way>((1 << 16) - 1);
	sync();
	l2_cache_debug_reg(0);
}


void Genode::Pl310::invalidate()
{
	write<Invalidate_by_way>((1 << 16) - 1);
	sync();
}
