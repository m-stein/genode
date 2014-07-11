/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \author Stefan Kalkowski
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

/* core includes */
#include <spec/arm_gic/pic_support.h>
#include <processor_driver.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Arm_gic
{
	public:

		/**
		 * Constructor
		 */
		Pic() : Arm_gic::Pic(Processor_driver::PL390_DISTRIBUTOR_MMIO_BASE,
		                     Processor_driver::PL390_CPU_MMIO_BASE)
		{
			/* configure every shared peripheral interrupt */
			for (unsigned i=MIN_SPI; i <= _max_interrupt; i++) {
				_distr.write<Distr::Icfgr::Edge_triggered>(0, i);
				_distr.write<Distr::Ipriorityr::Priority>(0, i);
				_distr.write<Distr::Itargetsr::Cpu_targets>(
					Distr::Itargetsr::ALL, i);
			}

			/* disable the priority filter */
			_cpu.write<Cpu::Pmr::Priority>(0xff);

			/* signal secure IRQ via FIQ interface */
			_cpu.write<Cpu::Ctlr>(Cpu::Ctlr::Enable_grp0::bits(1)  |
			                      Cpu::Ctlr::Enable_grp1::bits(1) |
			                      Cpu::Ctlr::Fiq_en::bits(1));

			/* use whole band of prios */
			_cpu.write<Cpu::Bpr::Binary_point>(Cpu::Bpr::NO_PREEMPTION);

			/* enable device */
			_distr.write<Distr::Ctlr>(Distr::Ctlr::Enable::bits(1));
		}

		/**
		 * Mark interrupt 'i' unsecure
		 */
		void unsecure(unsigned const i) {
			_distr.write<Distr::Igroupr::Group_status>(1, i); }
};


bool Genode::Arm_gic::_use_security_ext() { return 1; }


namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
