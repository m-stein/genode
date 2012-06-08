/*
 * \brief   Session component of a 32-bit-adder-emulation service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__ADDER32__ADDER32_SESSION_COMPONENT_H_
#define _SRC__ADDER32__ADDER32_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/printf.h>

/* Local includes */
#include <emulator_session/emulator_session.h>

namespace Genode
{
	/**
	 * Session component of a 32-bit-adder-emulation service
	 */
	class Adder32_session_component :
		public Genode::Rpc_object<Emulator::Session>
	{
		/* MMIO endianness types */
		enum Endianness { BIG_ENDIAN };

		/* MMIO attributes */
		enum {
			ADDEND1_BASE = 0*sizeof(umword_t),
			ADDEND1_SIZE = sizeof(umword_t),
			ADDEND2_BASE = 1*sizeof(umword_t),
			ADDEND2_SIZE = sizeof(umword_t),
			SUM_BASE = 2*sizeof(umword_t),
			MMIO_SIZE = 3*sizeof(umword_t),
			ENDIANNESS = BIG_ENDIAN,
		};

		/* MMIO space plus one word at the end for overhanging writes */
		char _mmio[MMIO_SIZE + sizeof(umword_t)];

		/* MMIO registers */
		umword_t * const _addend1;
		umword_t * const _addend2;
		umword_t * const _sum;

		/**
		 * Update sum register value
		 */
		void _update_sum() { *_sum = *_addend1 + *_addend2; }

		public:

			/**
			 * Constructor
			 */
			Adder32_session_component()
			:
				_addend1((umword_t *)&_mmio[ADDEND1_BASE]),
				_addend2((umword_t *)&_mmio[ADDEND2_BASE]),
				_sum((umword_t *)&_mmio[SUM_BASE])
			{ }

			/*********************
			 ** Adder32_session **
			 *********************/

			void write_mmio(addr_t const off, Access const a,
			                umword_t const value)
			{
				/* Check arguments */
				using namespace Genode;
				if (off > sizeof(_mmio)/sizeof(_mmio[0])) {
					PERR("%s: Invalid arguments", __PRETTY_FUNCTION__);
					while(1);
				}
				/* Write targeted bits with specific endianness to MMIO */
				switch(ENDIANNESS)
				{
				case BIG_ENDIAN: {
					switch(a)
					{
					case LSB32: {
						uint32_t * const dest = (uint32_t *)&_mmio[off];
						uint32_t * const src =
							(uint32_t *)&value +
							sizeof(umword_t)/sizeof(uint32_t) - 1;
						*dest=*src;
						break; }
					case LSB16: {
						uint16_t * const dest = (uint16_t *)&_mmio[off];
						uint16_t * const src =
							(uint16_t *)&value +
							sizeof(umword_t)/sizeof(uint16_t) - 1;
						*dest=*src;
						break; }
					case LSB8: {
						uint8_t * dest = (uint8_t *)&_mmio[off];
						uint8_t * src = (uint8_t *)&value +
						                sizeof(umword_t)/sizeof(uint8_t) - 1;
						*dest=*src;
						break; }
					default: {
						PERR("%s: Invalid access type", __PRETTY_FUNCTION__);
						while(1); }
					}
					break; }
				default: {
					PERR("%s: Invalid enianess", __PRETTY_FUNCTION__);
					while(1); }
				}
				/* Update device state */
				_update_sum();
			}

			umword_t read_mmio(addr_t const off, Access const a)
			{
				/* Check arguments */
				using namespace Genode;
				if (off > sizeof(_mmio)/sizeof(_mmio[0])) {
					PERR("%s: Invalid arguments", __PRETTY_FUNCTION__);
					while(1);
				}
				/* Read targeted bits from MMIO and return them */
				switch(ENDIANNESS)
				{
				case BIG_ENDIAN: {
					switch(a)
					{
					case LSB32: {
						uint32_t * src = (uint32_t *)&_mmio[off];
						return (umword_t)*src; }
					case LSB16: {
						uint16_t * src = (uint16_t *)&_mmio[off];
						return (umword_t)*src; }
					case LSB8: {
						uint8_t * src = (uint8_t *)&_mmio[off];
						return (umword_t)*src; }
					default: {
						PERR("%s: Invalid access type", __PRETTY_FUNCTION__);
						while(1);
						return 0; }
					}
					break; }
				default: {
					PERR("%s: Invalid enianess", __PRETTY_FUNCTION__);
					while(1);
					return 0; }
				}
			}
	};
}


#endif /* _SRC__ADDER32__ADDER32_SESSION_COMPONENT_H_ */

