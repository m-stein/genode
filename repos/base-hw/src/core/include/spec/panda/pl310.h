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

/* Genode includes */
#include <util/mmio.h>

namespace Genode { struct Pl310; }

/**
 * Driver for the Pl310 L2-Cache Controller
 */
struct Genode::Pl310 : Mmio
{
	struct Control : Register <0x100, 32>
	{
		struct Enable : Bitfield<0,1> {};
	};

	struct Aux : Register<0x104, 32>
	{
		struct Associativity  : Bitfield<16,1> { };
		struct Way_size       : Bitfield<17,3> { };
		struct Share_override : Bitfield<22,1> { };
		struct Reserved       : Bitfield<25,1> { };
		struct Ns_lockdown    : Bitfield<26,1> { };
		struct Ns_irq_ctrl    : Bitfield<27,1> { };
		struct Data_prefetch  : Bitfield<28,1> { };
		struct Inst_prefetch  : Bitfield<29,1> { };
		struct Early_bresp    : Bitfield<30,1> { };

		static access_t init_value()
		{
			access_t v = 0;
			Associativity::set(v, 1);
			Way_size::set(v, 3);
			Share_override::set(v, 1);
			Reserved::set(v, 1);
			Ns_lockdown::set(v, 1);
			Ns_irq_ctrl::set(v, 1);
			Data_prefetch::set(v, 1);
			Inst_prefetch::set(v, 1);
			Early_bresp::set(v, 1);
			return v;
		}
	};

	struct Debug : Register<0xf40, 32>
	{
		struct Dcl : Bitfield<0,1> { };
		struct Dwb : Bitfield<1,1> { };
	};

	struct Irq_mask                : Register <0x214, 32> {};
	struct Irq_clear               : Register <0x220, 32> {};
	struct Cache_sync              : Register <0x730, 32> {};

	template <off_t OFF> struct By_way : Register<OFF, 32>
	{
		struct Way_bits : Register<OFF, 32>::template Bitfield<0,16> {};
	};

	struct Invalidate_by_way       : By_way<0x77c> {};
	struct Clean_invalidate_by_way : By_way<0x7fc> {};

	inline void sync() { while (read<Cache_sync>()) ; }

	void invalidate();

	void flush();

	Pl310(addr_t const base);
};

namespace Genode {
	void pl310_enable(bool const v);
	void pl310_aux(Pl310::Aux::access_t const v);
	void pl310_debug(Pl310::Debug::access_t const v);
}
