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

namespace Board { struct Pic; }

namespace Kernel { void initialize_irq_controller_pkg(); }

struct Board::Pic : Kernel::Opaque_ada_type<Pic, 8>
{
	Pic();

	void take_request(unsigned &irq);

	bool request_was_taken() const;

	void finish_request();

	void unmask(unsigned const irq_id,
	            unsigned const cpu_id) const;

	void mask(unsigned const irq_id) const;

	void irq_mode(unsigned const irq_id,
	              unsigned const trigger_mode,
	              unsigned const polarity);

	void store_apic_id(unsigned const cpu_id);

	void send_ipi(unsigned const cpu_id) const;

	static unsigned nr_of_irqs();

	static unsigned ipi();
};

#endif /* _CORE__SPEC__X86_64__PIC_H_ */
