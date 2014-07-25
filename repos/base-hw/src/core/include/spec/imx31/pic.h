/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Mmio
{
	/**
	 * Interrupt control register
	 */
	struct Intcntl : Register<0x0, 32>
	{
		struct Nm : Bitfield<18,1> /* IRQ mode */
		{
			enum { SW_CONTROL = 0 };
		};

		struct Fiad   : Bitfield<19,1> { }; /* FIQ rises prio in arbiter */
		struct Niad   : Bitfield<20,1> { }; /* IRQ rises prio in arbiter */
		struct Fidis  : Bitfield<21,1> { }; /* FIQ disable */
		struct Nidis  : Bitfield<22,1> { }; /* IRQ disable */
		struct Abfen  : Bitfield<24,1> { }; /* if ABFLAG is sticky */
		struct Abflag : Bitfield<25,1> { }; /* rise prio in bus arbiter */

		static access_t init_value()
		{
			return Nm::bits(Nm::SW_CONTROL) |
			       Fiad::bits(0) |
			       Niad::bits(0) |
			       Fidis::bits(0) |
			       Nidis::bits(0) |
			       Abfen::bits(0) |
			       Abflag::bits(0);
		}
	};

	/**
	 * Normal interrupt mask register
	 */
	struct Nimask : Register<0x4, 32>
	{
		enum { NONE_MASKED = ~0 };
	};

	/**
	 * Interrupt enable number register
	 */
	struct Intennum : Register<0x8, 32>
	{
		struct Enable : Bitfield<0,6> { };
	};

	/**
	 * Interrupt disable number register
	 */
	struct Intdisnum : Register<0xc, 32>
	{
		struct Disable : Bitfield<0,6> { };
	};

	/**
	 * Interrupt enable register
	 */
	struct Intenableh : Register<0x10, 32> { };
	struct Intenablel : Register<0x14, 32> { };

	/**
	 * Interrupt type register
	 */
	struct Inttype { enum { ALL_IRQS = 0 }; };
	struct Inttypeh : Register<0x18, 32> { };
	struct Inttypel : Register<0x1c, 32> { };

	/**
	 * Normal interrupt priority registers
	 */
	struct Nipriority : Register_array<0x20, 32, 8, 32>
	{
		enum { ALL_LOWEST = 0 };
	};

	/**
	 * Interrupt source registers
	 */
	struct Intsrch : Register<0x48, 32> { };
	struct Intsrcl : Register<0x4c, 32> { };

	/**
	 * Normal interrupt pending registers
	 */
	struct Nipndh : Register<0x58, 32> { };
	struct Nipndl : Register<0x5c, 32> { };

	/**
	 * Normal interrupt vector and status register
	 */
	struct Nivecsr : Register<0x40, 32>
	{
		struct Nvector : Bitfield<16, 16> { };
	};

	public:

		enum { NR_OF_IRQ = 64 };

		/**
		 * Constructor, enables all interrupts
		 */
		Pic() : Mmio(Board::AVIC_MMIO_BASE)
		{
			mask();
			write<Nimask>(Nimask::NONE_MASKED);
			write<Intcntl>(Intcntl::init_value());
			write<Inttypeh>(Inttype::ALL_IRQS);
			write<Inttypel>(Inttype::ALL_IRQS);
			for (unsigned i = 0; i < Nipriority::ITEMS; i++)
				write<Nipriority>(Nipriority::ALL_LOWEST, i);
		}

		/**
		 * Initialize processor local interface of the controller
		 */
		void init_processor_local() { }

		/**
		 * Receive a pending request number 'i'
		 */
		bool take_request(unsigned & i)
		{
			i = read<Nivecsr::Nvector>();
			return valid(i) ? true : false;
		}

		/**
		 * Finish the last taken request
		 */
		void finish_request() {
			/* requests disappear by source retraction or masking */ }

		/**
		 * Validate request number 'i'
		 */
		bool valid(unsigned const i) const {
			return i < NR_OF_IRQ; }

		/**
		 * Mask all interrupts
		 */
		void mask()
		{
			write<Intenablel>(0);
			write<Intenableh>(0);
		}

		/**
		 * Unmask interrupt
		 *
		 * \param interrupt_id  kernel name of targeted interrupt
		 */
		void unmask(unsigned const interrupt_id, unsigned)
		{
			if (interrupt_id < NR_OF_IRQ) {
				write<Intennum>(interrupt_id);
			}
		}

		/**
		 * Mask interrupt 'i'
		 */
		void mask(unsigned const i) {
			if (i < NR_OF_IRQ) write<Intdisnum>(i); }

		/**
		 * Wether an interrupt is inter-processor interrupt of a processor
		 *
		 * \param interrupt_id  kernel name of the interrupt
		 * \param processor_id  kernel name of the processor
		 */
		bool is_ip_interrupt(unsigned const interrupt_id,
		                     unsigned const processor_id)
		{
			return false;
		}

		/**
		 * Trigger the inter-processor interrupt of a processor
		 *
		 * \param processor_id  kernel name of the processor
		 */
		void trigger_ip_interrupt(unsigned const processor_id) { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
