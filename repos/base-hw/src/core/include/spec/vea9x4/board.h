/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/stdint.h>

/* core includes */
#include <drivers/board_base.h>

namespace Genode
{
	class Board : public Board_base
	{
		public:
			static bool is_smp() { return 1; }
			static void outer_cache_invalidate();
			static void outer_cache_flush();
			static void prepare_kernel() { }
			static void secondary_cpus_ip(void * const ip);
			static void init_mp_sync(unsigned const cpu);
			static void init_mp_async(bool const primary, addr_t const core_tt,
			                          unsigned const core_id);
			static void raise_actlr_smp_bit();
	};

	class Pl310;

	Pl310 * l2_cache();
}

enum {
	ENABLE_L2_CACHE = 0,
	ENABLE_SCU = 0,
	ENABLE_ACTLR_SMP = 0,
};

class Genode::Pl310 : public Genode::Mmio
{
	private:

		struct Control : Register<0x100, 32> {
			struct Enable : Bitfield<0,1> { }; };

		struct Debug : Register<0xf40, 32>
		{
			struct Dcl : Bitfield<0,1> { };
			struct Dwb : Bitfield<1,1> { };
		};

		struct Cache_sync : Register<0x730, 32> {
			struct C : Bitfield<0, 1> { }; };

		struct Invalidate_by_way : Register<0x77c, 32> {
			struct Way_bits : Bitfield<0, 16> { }; };

		struct Clean_invalidate_by_way : Register<0x7fc, 32> {
			struct Way_bits : Bitfield<0, 16> { }; };

		struct Irq_mask : Register<0x214, 32> { };
		struct Irq_clear : Register<0x220, 32> { };

		void _sync() { while (read<Cache_sync::C>()) ; }

	public:

		Pl310(Genode::addr_t const base) : Mmio(base) {
			if (!ENABLE_L2_CACHE) { disable(); } }

		void disable() { write<Control::Enable>(0); }

		void enable()
		{
			if (!ENABLE_L2_CACHE) { return; }
			write<Control::Enable>(1);
			write<Irq_mask>(0);
			write<Irq_clear>(~0);
		}

		void flush()
		{
			if (!ENABLE_L2_CACHE) { return; }
			Debug::access_t debug = 0;
			Debug::Dcl::set(debug, 1);
			Debug::Dwb::set(debug, 1);
			write<Debug>(debug);
			write<Clean_invalidate_by_way::Way_bits>(~0);
			_sync();
			write<Debug>(0);
		}

		void invalidate()
		{
			if (!ENABLE_L2_CACHE) { return; }
			write<Invalidate_by_way::Way_bits>(~0);
			_sync();
		}
};

#endif /* _BOARD_H_ */
