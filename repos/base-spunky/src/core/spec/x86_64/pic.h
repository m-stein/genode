/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \date   2021-06-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__PIC_H_
#define _CORE__SPEC__X86_64__PIC_H_

/* Spunky includes */
#include <kernel/ada_interfacing.h>

namespace Board {

	struct Local_interrupt_controller;
	struct Global_interrupt_controller { };
}

namespace Kernel { void initialize_irq_controller_pkg(); }

struct Board::Local_interrupt_controller : Kernel::Opaque_ada_type<Local_interrupt_controller, 8>
{
	Local_interrupt_controller(Global_interrupt_controller &global_irq_ctrl);

	void finish_request();

	void unmask(unsigned const irq_id,
	            unsigned const cpu_id);

	void mask(unsigned const irq_id);

	void take_request(unsigned &irq,
	                  bool     &irq_valid);

	void irq_mode(unsigned irq,
	              unsigned trigger,
	              unsigned polarity);

	void store_apic_id(unsigned const cpu_id);

	void send_ipi(unsigned const cpu_id) const;

	static unsigned nr_of_irqs();
	static unsigned ipi();
};

#endif /* _CORE__SPEC__X86_64__PIC_H_ */
