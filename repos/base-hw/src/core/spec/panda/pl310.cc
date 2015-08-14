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
	pl310_aux(Aux::init_value());
}


void Genode::Pl310::flush()
{
	Debug::access_t debug = 0;
	Debug::Dwb::set(debug, 1);
	Debug::Dcl::set(debug, 1);
	pl310_debug(debug);

	write<Clean_invalidate_by_way::Way_bits>(~0);
	sync();
	pl310_debug(0);
}


void Genode::Pl310::invalidate()
{
	write<Invalidate_by_way::Way_bits>(~0);
	sync();
}


void Genode::Pl310::disable()
{
	pl310_enable(false);
}


void Genode::Pl310::enable()
{
	pl310_enable(true);
	write<Irq_mask>(0);
	write<Irq_clear>(~0);
}
