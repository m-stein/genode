/*
 * \brief  Kernel cpu object implementations for multiprocessor systems
 * \author Stefan Kalkowski
 * \date   2018-11-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>

using namespace Kernel;


void Inter_processor_irq::occurred()
{
	/* iterate through the local and global work-list */
	_cpu._local_work_list.execute_each();
	_cpu._global_work_list.execute_each();

	/* mark the inter-processor IRQ as being received */
	_pending = false;
}


void Cpu::trigger_ip_interrupt()
{
	/* check whether there is still an inter-processor IRQ send */
	if (_inter_processor_irq.pending())
		return;

	_pic.send_ipi(_id);
	_inter_processor_irq.pending(true);
}


Inter_processor_irq::Inter_processor_irq(Cpu &cpu)
:
	_irq { Board::Pic::ipi(), cpu.irq_pool(), cpu.pic(), *this },
	_cpu { cpu }
{
	_cpu.pic().unmask(Board::Pic::ipi(), _cpu.id());
}
