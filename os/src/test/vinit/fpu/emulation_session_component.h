/*
 * \brief   Session component of the emulation service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FPU__EMULATION_SESSION_COMPONENT_H_
#define _FPU__EMULATION_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/printf.h>
#include <base/thread.h>
#include <base/signal.h>
#include <util/register.h>

/* local includes */
#include <emulation_session/emulation_session.h>

namespace Emulation
{
	/**
	 * Session component of the emulation service
	 */
	class Session_component : public Rpc_object<Session>
	{
		/**
		 * MMIO endianness types
		 */
		enum Endianness { BIG_ENDIAN };

		enum {
			MMIO_ENDIANNESS = BIG_ENDIAN,
			MMIO_1_BASE = 0x0,
			MMIO_2_BASE = 0x1000,
			MMIO_SIZE = 0x2000,
			IRQ_CALC_DONE = 1,
			CALC_THREAD_STACK_SIZE = 4 * 1024,
		};

		/**
		 * Structure of first emulated MMIO region
		 */
		class Mmio_1 : public Mmio
		{
			public:

				/**
				 * Control register
				 */
				struct Ctrl : Register<0x0, 32>
				{
					struct Mode : Bitfield<0, 1> /* calulation mode */
					{
						enum { ADD = 0, SUBTRACT = 1 };
					};
					struct Calc : Bitfield<1, 1> { }; /* calculation active bit */
					struct Clr_irq : Bitfield<2, 1> { }; /* clear interrupt */
				};

				/**
				 * Constructor
				 */
				Mmio_1(addr_t const base) : Mmio(base) { }
		};

		/**
		 * Structure of second emulated MMIO region
		 */
		class Mmio_2 : public Mmio
		{
			public:

				/**
				 * Calculation argument 1
				 */
				struct Arg_1 : Register<0x0, 32> { };

				/**
				 * Calculation argument 2
				 */
				struct Arg_2 : Register<0x4, 32> { };

				/**
				 * Calculation result
				 */
				struct Result : Register<0x8, 32> { };

				/**
				 * Constructor
				 */
				Mmio_2(addr_t const base) : Mmio(base) { }
		};

		/**
		 * IRQ output line
		 */
		class Irq
		{
			bool _state; /* output state */
			Signal_context_capability _handler; /* IRQ edge handler */

			/**
			 * Negate the IRQ output
			 */
			void _edge()
			{
				_state = !_state;
				if (_handler.valid()) {
					Signal_transmitter(_handler).submit();
				}
			}

			public:

				/**
				 * Constructor
				 */
				Irq() : _state(0) { }

				/**
				 * Raise output if it is 0
				 */
				void up() { if (!_state) _edge(); }

				/**
				 * Level down output if it is 1
				 */
				void down() { if (_state) _edge(); }

				/***************
				 ** Accessors **
				 ***************/

				bool state() const { return _state; }

				void handler(Signal_context_capability handler)
				{ _handler = handler; }
		};

		/**
		 * IRQ output line with synchronized access
		 */
		class Synchronized_irq
		{
			Irq _irq; /* asynchronous IRQ output */
			Lock _lock; /* synchronize access to '_irq' */

			public:

				/*******************
				 ** Irq interface **
				 *******************/

				void handler(Signal_context_capability handler)
				{
					Lock::Guard guard(_lock);
					_irq.handler(handler);
				}

				bool state()
				{
					Lock::Guard guard(_lock);
					return _irq.state();
				}

				void up()
				{
					Lock::Guard guard(_lock);
					_irq.up();
				}

				void down()
				{
					Lock::Guard guard(_lock);
					_irq.down();
				}
		};

		/**
		 * Thread that does FPU calculations in parallel with client access
		 */
		class Calculation_thread : public Thread<CALC_THREAD_STACK_SIZE>
		{
			Session_component * const _emu; /* our creator */

			public:

				/**
				 * Construct a valid calculation thread
				 */
				Calculation_thread(Session_component * const emu)
				: _emu(emu) { }


				/**********************
				 ** Thread interface **
				 **********************/

				void entry()
				{
					while (1)
					{
						/* wait for a calculation request by the emulator */
						while (_emu->_receiver.wait_for_signal().context() !=
						       &(_emu->_calc_up)) ;

						/* buffer MMIO state needed for the calculation */
						unsigned const arg1 = _emu->_mmio_2.read<Mmio_2::Arg_1>();
						unsigned const arg2 = _emu->_mmio_2.read<Mmio_2::Arg_2>();
						unsigned const ctrl = _emu->_mmio_1.read<Mmio_1::Ctrl>();

						/* let RPC entrypoint continue MMIO access */
						Signal_transmitter(_emu->_ack_calc_up_cap).submit();

						/* do calculation */
						unsigned result;
						switch (Mmio_1::Ctrl::Mode::get(ctrl)) {
						case Mmio_1::Ctrl::Mode::SUBTRACT:
							result = arg1 - arg2;
							break;
						case Mmio_1::Ctrl::Mode::ADD:
							result = arg1 + arg2;
							break;
						default:
							break;
						}
						/* writeback results and announce end of calculation */
						_emu->_mmio_2.write<Mmio_2::Result>(result);
						_emu->_mmio_1.write<Mmio_1::Ctrl::Calc>(0);
						_emu->_calc_done.up();
					}
				}
		};

		/* MMIO backing store plus 1 word at the end for overhanging writes */
		char _mmio[MMIO_SIZE + sizeof(umword_t)];

		Mmio_1 _mmio_1;
		Mmio_2 _mmio_2;
		Synchronized_irq _calc_done;
		bool _old_calc;
		Signal_context _calc_up;
		Signal_context _ack_calc_up;
		Signal_receiver _receiver;
		Signal_context_capability _calc_up_cap;
		Signal_context_capability _ack_calc_up_cap;
		Calculation_thread _calc_thread;

		/**
		 * Gets called when the emulator attempts to write to the MMIO
		 */
		void _before_write_mmio()
		{
			/* enable detection of calc-bit edges */
			_old_calc = _mmio_1.read<Mmio_1::Ctrl::Calc>();
		}

		/**
		 * Gets called after the emulator has written to the MMIO
		 */
		void _after_write_mmio()
		{
			/* process requests to clear the IRQ output */
			if (_mmio_1.read<Mmio_1::Ctrl::Clr_irq>()) {
				_calc_done.down();
				_mmio_1.write<Mmio_1::Ctrl::Clr_irq>(0);
			}
			/* start calculation when calc bit goes up */
			bool new_calc = _mmio_1.read<Mmio_1::Ctrl::Calc>();
			if (!_old_calc && new_calc) {
				Signal_transmitter(_calc_up_cap).submit();
				while (_receiver.wait_for_signal().context() != &_ack_calc_up) ;
			}
			/* ensure that calc bit can't go down by a client write-access */
			else if (_old_calc && !new_calc)
				_mmio_1.write<Mmio_1::Ctrl::Calc>(1);
		}

		public:

			/**
			 * Construct a valid session component
			 */
			Session_component() :
				_mmio_1((addr_t)&_mmio[MMIO_1_BASE]),
				_mmio_2((addr_t)&_mmio[MMIO_2_BASE]),
				_calc_up_cap(_receiver.manage(&_calc_up)),
				_ack_calc_up_cap(_receiver.manage(&_ack_calc_up)),
				_calc_thread(this)
			{
				/* zero-fill MMIO and start the calculation thread */
				for (unsigned i = 0; i < sizeof(_mmio)/sizeof(_mmio[0]); i++) _mmio[i] = 0;
				_calc_thread.start();
			}


			/***********************
			 ** Session interface **
			 ***********************/

			void write_mmio(addr_t const off, Access const a,
			                umword_t const value)
			{
				/* initialize access */
				_before_write_mmio();

				/* check arguments */
				if (off > sizeof(_mmio)/sizeof(_mmio[0])) {
					PERR("%s:%d: Invalid offset", __FILE__, __LINE__);
					return;
				}
				/* write targeted bits with specific endianness to MMIO */
				switch (MMIO_ENDIANNESS) {
				case BIG_ENDIAN: {
					switch (a) {
					case LSB32: {
						uint32_t * const dest = (uint32_t *)&_mmio[off];
						uint32_t * const src =
							(uint32_t *)&value +
							sizeof(umword_t)/sizeof(uint32_t) - 1;
						*dest = *src;
						break; }
					case LSB16: {
						uint16_t * const dest = (uint16_t *)&_mmio[off];
						uint16_t * const src =
							(uint16_t *)&value +
							sizeof(umword_t)/sizeof(uint16_t) - 1;
						*dest = *src;
						break; }
					case LSB8: {
						uint8_t * const dest = (uint8_t *)&_mmio[off];
						uint8_t * const src =
							(uint8_t *)&value +
							sizeof(umword_t)/sizeof(uint8_t) - 1;
						*dest = *src;
						break; }
					default: {
						PERR("%s:%d: Invalid access type", __FILE__, __LINE__);
						break; }
					}
					break; }
				default: {
					PERR("%s:%d: Invalid endianess", __FILE__, __LINE__);
					break; }
				}
				/* finish access */
				_after_write_mmio();
			}

			umword_t read_mmio(addr_t const off, Access const a)
			{
				/* check arguments */
				if (off > sizeof(_mmio)/sizeof(_mmio[0])) {
					PERR("%s: Invalid arguments", __PRETTY_FUNCTION__);
					while(1);
				}
				/* read targeted bits from MMIO and return them */
				switch (MMIO_ENDIANNESS) {
				case BIG_ENDIAN: {
					switch (a)
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
						PERR("%s:%d: Invalid access type", __FILE__, __LINE__);
						return 0; }
					}
					break; }
				default: {
					PERR("%s:%d: Invalid endianess", __FILE__, __LINE__);
					return 0; }
				}
			}

			bool irq_handler(unsigned const irq,
			                 Signal_context_capability irq_edge)
			{
				if (irq == IRQ_CALC_DONE) {
					_calc_done.handler(irq_edge);
					return _calc_done.state();
				}
				else PERR("%s:%d: Invalid IRQ", __FILE__, __LINE__);
				return 0;
			}
	};
}

#endif /* _FPU__EMULATION_SESSION_COMPONENT_H_ */

