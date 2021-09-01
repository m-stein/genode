/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/irq.h>

using namespace Genode;


Kernel::Irq::Irq(unsigned const  irq,
                 Irq::Pool      &irq_pool,
                 Board::Pic     &pic,
                 Timer_irq      &timer_irq)
:
	_irq_nr    { irq },
	_irq_pool  { irq_pool },
	_pic       { pic },
	_type      { Type::TIMER_IRQ },
	_typed_irq { (addr_t)&timer_irq }
{
	_irq_pool.insert(&_avl_node);
}


Kernel::Irq::Irq(unsigned const  irq,
                 Irq::Pool      &irq_pool,
                 Board::Pic     &pic,
                 User_irq       &user_irq)
:
	_irq_nr    { irq },
	_irq_pool  { irq_pool },
	_pic       { pic },
	_type      { Type::USER_IRQ },
	_typed_irq { (addr_t)&user_irq }
{
	_irq_pool.insert(&_avl_node);
}


Kernel::Irq::Irq(unsigned const       irq,
                 Irq::Pool           &irq_pool,
                 Board::Pic          &pic,
                 Inter_processor_irq &inter_processor_irq)
:
	_irq_nr    { irq },
	_irq_pool  { irq_pool },
	_pic       { pic },
	_type      { Type::INTER_PROCESSOR_IRQ },
	_typed_irq { (addr_t)&inter_processor_irq }
{
	_irq_pool.insert(&_avl_node);
}


void Kernel::Irq::occurred()
{
	switch (_type) {
	case Type::TIMER_IRQ:
		((Timer_irq *)_typed_irq)->occurred();
		break;
	case Type::USER_IRQ:
		((User_irq *)_typed_irq)->occurred();
		break;
	case Type::INTER_PROCESSOR_IRQ:
		((Inter_processor_irq *)_typed_irq)->occurred();
		break;
	}
}


Kernel::User_irq *Kernel::Irq::user_irq()
{
	switch (_type) {
	case Type::USER_IRQ:
		return (User_irq *)_typed_irq;
	default:
		break;
	}
	return nullptr;
}


void Kernel::Irq::disable() const
{
	_pic.mask(_irq_nr);
}


void Kernel::Irq::enable() const
{
	_pic.unmask(_irq_nr, Genode::Cpu::executing_id());
}


Kernel::User_irq::User_irq(unsigned                const  irq,
                           Genode::Irq_session::Trigger   trigger,
                           Genode::Irq_session::Polarity  polarity,
                           Signal_context                &context,
                           Board::Pic                    &pic,
                           Irq::Pool                     &user_irq_pool)
:
	_irq     { irq, user_irq_pool, pic, *this },
	_context { context }
{
	_irq.disable();
	pic.irq_mode(irq, trigger, polarity);
}
