/*
 * \brief   Instruction decoding specific to ARM V7A
 * \author  Martin Stein
 * \date    2012-04-17
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VINIT__ARM_V7A__INSTRUCTION_H_
#define _VINIT__ARM_V7A__INSTRUCTION_H_

/* Genode includes */
#include <util/register.h>
#include <rm_session/rm_session.h>

namespace Init
{
	using namespace Genode;

	/**
	 * ARM V7A instruction decoding as far as needed
	 */
	struct Instruction
	{
		enum { WIDTH = 32 };

		/**
		 * Encodings for the whole instruction space
		 */
		struct Code : Genode::Register<WIDTH>
		{
			struct C1 : Bitfield<26, 2> { };
			struct C2 : Bitfield<25, 1> { };
			struct C3 : Bitfield<4, 1> { };

			/**
			 * If 'code' is of type 'Code_ld_st_w_ub'
			 */
			static bool ld_st_w_ub(access_t const code)
			{
				if (C1::get(code) != 1) return 0;
				if (C2::get(code) != 0) if (C3::get(code) != 0) return 0;
				return 1;
			}
		};

		/**
		 * Encodings of type "Load/store word and unsigned byte"
		 */
		struct Code_ld_st_w_ub : Genode::Register<WIDTH>
		{
			struct C1 : Bitfield<20, 1> { };
			struct C2 : Bitfield<22, 1> { };
			struct C3 : Bitfield<21, 1> { };
			struct C4 : Bitfield<24, 1> { };

			/**
			 * If 'code' is of type "Store register"
			 */
			static bool str(access_t const code)
			{
				if (C1::get(code) != 0) return 0;
				if (C2::get(code) != 0) return 0;
				if (C3::get(code) == 1) if (C4::get(code) == 0) return 0;
				return 1;
			}

			/**
			 * If 'code' is of type "Load register"
			 */
			static bool ldr(access_t const code)
			{
				if (C1::get(code) != 1) return 0;
				if (C2::get(code) != 0) return 0;
				if (C3::get(code) == 1) if (C4::get(code) == 0) return 0;
				return 1;
			}

			/**
			 * If 'code' is of type "Store register byte"
			 */
			static bool strb(access_t const code)
			{
				if (C1::get(code) != 0) return 0;
				if (C2::get(code) != 1) return 0;
				if (C3::get(code) == 1) if (C4::get(code) == 0) return 0;
				return 1;
			}

			/**
			 * If 'code' is of type "Load register byte"
			 */
			static bool ldrb(access_t const code)
			{
				if (C1::get(code) != 1) return 0;
				if (C2::get(code) != 1) return 0;
				if (C3::get(code) == 1) if (C4::get(code) == 0) return 0;
				return 1;
			}
		};

		/**
		 * Type 1 encoding of the "Rt"-register ID field in instructions
		 */
		struct Rt_1 : Genode::Register<WIDTH>
		{
			struct Rt : Bitfield<12, 4> { };
		};

		struct Code_ldr : Rt_1 { };
		struct Code_str : Rt_1 { };
		struct Code_strb : Rt_1 { };
		struct Code_ldrb : Rt_1 { };

		/**
		 * If 'code' is a STR instruction get its attributes
		 */
		static bool str(Code::access_t const code, bool & writes,
		                Rm_session::Access_format & format, unsigned & reg)
		{
			bool const ret = Code::ld_st_w_ub(code) &&
			                 Code_ld_st_w_ub::str(code);
			if (ret) {
				reg = Code_str::Rt::get(code);
				writes = 1;
				format = Rm_session::LSB32;
			}
			return ret;
		}

		/**
		 * If 'code' is a LDR instruction get its attributes
		 */
		static bool ldr(Code::access_t const code, bool & writes,
		                Rm_session::Access_format & format, unsigned & reg)
		{
			bool const ret = Code::ld_st_w_ub(code) &&
			                 Code_ld_st_w_ub::ldr(code);
			if (ret) {
				reg = Code_str::Rt::get(code);
				writes = 0;
				format = Rm_session::LSB32;
			}
			return ret;
		}

		/**
		 * If 'code' is a STRB instruction get its attributes
		 */
		static bool strb(Code::access_t const code, bool & writes,
		                 Rm_session::Access_format & format, unsigned & reg)
		{
			if (!Code::ld_st_w_ub(code)) return 0;
			if (!Code_ld_st_w_ub::strb(code)) return 0;
			reg = Code_strb::Rt::get(code);
			writes = 1;
			format = Rm_session::LSB8;
			return 1;
		}

		/**
		 * If 'code' is a LDRB instruction get its attributes
		 */
		static bool ldrb(Code::access_t const code, bool & writes,
		                 Rm_session::Access_format & format, unsigned & reg)
		{
			if (!Code::ld_st_w_ub(code)) return 0;
			if (!Code_ld_st_w_ub::ldrb(code)) return 0;
			reg = Code_ldrb::Rt::get(code);
			writes = 0;
			format = Rm_session::LSB8;
			return 1;
		}

		/**
		 * Size of an instruction
		 */
		static size_t size() { return sizeof(Code::access_t); }

		/**
		 * If 'code' is a load/store instruction get its attributes
		 */
		static bool load_store(unsigned const code, bool & writes,
		                       Rm_session::Access_format & format,
		                       unsigned & reg)
		{
			return Instruction::str(code, writes, format, reg) ||
			       Instruction::ldr(code, writes, format, reg) ||
			       Instruction::ldrb(code, writes, format, reg) ||
			       Instruction::strb(code, writes, format, reg);
		}
	};
}

#endif /* _VINIT__ARM_V7A__INSTRUCTION_H_ */

