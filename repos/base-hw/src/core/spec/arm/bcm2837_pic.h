/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2019-05-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__BCM2837_PIC_H_
#define _CORE__SPEC__ARM__BCM2837_PIC_H_

#include <util/mmio.h>

namespace Board {

	class Global_interrupt_controller { };
	class Pic;
}


class Board::Pic : Genode::Mmio
{
	private:

		enum {
			IPI       = 0,
			NR_OF_IRQ = 64,
		};

		template <unsigned CPU_NUM>
		struct Core_timer_irq_control : Register<0x40+CPU_NUM*0x4, 32>
		{
			struct Cnt_p_ns_irq
			: Register<0x40+CPU_NUM*0x4, 32>::template Bitfield<1, 1> {};
		};

		template <unsigned CPU_NUM>
		struct Core_mailbox_irq_control : Register<0x50+CPU_NUM*0x4, 32> {};

		template <unsigned CPU_NUM>
		struct Core_irq_source : Register<0x60+CPU_NUM*0x4, 32> {};

		template <unsigned CPU_NUM>
		struct Core_mailbox_set : Register<0x80+CPU_NUM*0x10, 32> {};

		template <unsigned CPU_NUM>
		struct Core_mailbox_clear : Register<0xc0+CPU_NUM*0x10, 32> {};

		void _ipi(unsigned cpu, bool enable);
		void _timer_irq(unsigned cpu, bool enable);

	public:

		Pic(Global_interrupt_controller &);

		/**
		 * Try to receive a pending IRQ
		 *
		 * \param irq        returns kernel name of received IRQ on success
		 * \param irq_valid  returns whether the call could receive an IRQ
		 */
		void take_request(unsigned &irq,
		                  bool     &irq_valid);

		void finish_request() { }
		void unmask(unsigned const i, unsigned);
		void mask(unsigned const i);
		void irq_mode(unsigned, unsigned, unsigned);
		void send_ipi(unsigned);

		static constexpr bool fast_interrupts() { return false; }

		static unsigned nr_of_irqs() { return NR_OF_IRQ; }
		static unsigned ipi()        { return IPI; }
};

#endif /* _CORE__SPEC__ARM__BCM2837_PIC_H_ */
