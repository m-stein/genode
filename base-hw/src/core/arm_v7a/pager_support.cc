/*
 * \brief   Pager parts that are specific to ARM V7A
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>
#include <util/register.h>

using namespace Genode;

enum { IP_REGISTER = 15 };

/**
 * ARMV7 instruction decoding as far as needed
 */
class Instruction
{
	enum { WIDTH = 32 };

	/**
	 * Encodings for the whole instruction space
	 */
	struct Code : Register<WIDTH>
	{
		struct C1 : Bitfield<26, 2> { };
		struct C2 : Bitfield<25, 1> { };
		struct C3 : Bitfield<4, 1> { };

		/**
		 * If 'code' is of type 'Code_ld_st_1'
		 */
		static bool ld_st_w_ub(access_t const code) {
			return C1::get(code) == 1 && (!C2::get(code) || !C3::get(code)); }
	};

	/**
	 * Encodings of type "Load/store word and unsigned byte"
	 */
	struct Code_ld_st_w_ub : Register<WIDTH>
	{
		struct C1 : Bitfield<20, 1> { };
		struct C2 : Bitfield<22, 1> { };
		struct C3 : Bitfield<20, 3> { };
		struct C4 : Bitfield<24, 1> { };

		/**
		 * If 'code' is of type 'Code_ldr_str' and "Store register"
		 */
		static bool str(access_t const code)
		{
			return !C1::get(code) && !C2::get(code) &&
			       !((C3::get(code) == 2) && !C4::get(code));
		}

		/**
		 * If 'code' is of type 'Code_ldr_str' and "Load register"
		 */
		static bool ldr(access_t const code)
		{
			return C1::get(code) && !C2::get(code) &&
			       !((C3::get(code) == 3) && !C4::get(code));
		}
	};

	/**
	 * Encodings of type "Load/store register"
	 */
	struct Code_ldr_str : Register<WIDTH>
	{
		struct Rt : Bitfield<12, 4> { };
	};

	public:

		/**
		 * If 'code' is a STR instruction get the source register
		 */
		static bool str(Code::access_t const c, unsigned & reg)
		{
			bool const r = Code::ld_st_w_ub(c) &&
			               Code_ld_st_w_ub::str(c);
			if (r) reg = Code_ldr_str::Rt::get(c);
			return r;
		}

		/**
		 * If 'code' is a LDR instruction get the target register
		 */
		static bool ldr(Code::access_t const c, unsigned & reg)
		{
			bool const r = Code::ld_st_w_ub(c) &&
			               Code_ld_st_w_ub::ldr(c);
			if (r) reg = Code_ldr_str::Rt::get(c);
			return r;
		}

		/**
		 * Size of an instruction
		 */
		static size_t size() { return sizeof(Code::access_t); }
};


namespace Genode
{
	bool instruction_writes_word(unsigned long * const code,
	                             unsigned long & value,
	                             unsigned long const thread_id)
	{
		/* Determine if the instruction attempts to write */
		unsigned reg;
		if (!Instruction::str(*code, reg)) return 0;

		/* Read the value that shall be written by the instruction */
		value = Kernel::read_register(thread_id, reg);
		return 1;
	}
}


void Genode::Pager_object::instruction_processed(unsigned long * const code,
                                                 unsigned long const read)
{
	/* If the instruction has read a value write it into the target register */
	unsigned reg;
	if (Instruction::ldr(*code, reg)) Kernel::write_register(_badge, reg, read);

	/* Adjust the faulters IP to the next instruction */
	addr_t ip = Kernel::read_register(_badge, IP_REGISTER);
	ip += Instruction::size();
	Kernel::write_register(_badge, IP_REGISTER, ip);
}

