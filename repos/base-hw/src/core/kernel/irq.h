/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IRQ_H_
#define _CORE__KERNEL__IRQ_H_

/* Genode includes */
#include <irq_session/irq_session.h>
#include <util/avl_tree.h>

/* core includes */
#include <kernel/signal_receiver.h>

namespace Board {

	class Pic;
}


namespace Kernel {

	class Timer_irq;
	class Inter_processor_irq;

	/**
	 * Kernel back-end interface of an interrupt
	 */
	class Irq;

	/**
	 * Kernel back-end of a user interrupt
	 */
	class User_irq;
}


namespace Genode {

	/**
	 * Core front-end of a user interrupt
	 */
	class Irq;
}


class Kernel::Irq
{
	public:

		struct Pool : Genode::Avl_tree<Genode::Avl_node_member<Irq> >
		{
			Irq * object(unsigned const id) const
			{
				Genode::Avl_node_member<Irq> * const node = first();
				if (!node) {
					return nullptr;
				}
				return node->object().find(id);
			}
		};

	private:

		enum class Type { TIMER_IRQ, USER_IRQ, INTER_PROCESSOR_IRQ };

		Genode::Avl_node_member<Irq>  _avl_node { *this };
		unsigned               const  _irq_nr; /* kernel name of the interrupt */
		Pool                         &_irq_pool;
		Board::Pic                   &_pic;
		Type                   const  _type;
		Genode::addr_t         const  _typed_irq;

	public:

		Irq(unsigned const  irq,
		    Pool           &irq_pool,
		    Board::Pic     &pic,
		    Timer_irq      &timer_irq);

		Irq(unsigned const  irq,
		    Pool           &irq_pool,
		    Board::Pic     &pic,
		    User_irq      &user_irq);

		Irq(unsigned const       irq,
		    Pool                &irq_pool,
		    Board::Pic          &pic,
		    Inter_processor_irq &inter_processor_irq);

		~Irq() { _irq_pool.remove(&_avl_node); }

		/**
		 * Handle occurence of the interrupt
		 */
		void occurred();

		/**
		 * Prevent interrupt from occurring
		 */
		void disable() const;

		/**
		 * Allow interrupt to occur
		 */
		void enable() const;

		unsigned irq_number() { return _irq_nr; }


		/************************
		 * 'Avl_node' interface *
		 ************************/

		bool higher(Irq * i) const { return i->_irq_nr > _irq_nr; }

		/**
		 * Find irq with 'nr' within this AVL subtree
		 */
		Irq * find(unsigned const nr)
		{
			if (nr == _irq_nr) return this;
			Genode::Avl_node_member<Irq> * const subtree =
				_avl_node.child(nr > _irq_nr);

			return (subtree) ? subtree->object().find(nr): nullptr;
		}

		User_irq *user_irq();
};


class Kernel::User_irq
{
	private:

		Irq             _irq;
		Kernel::Object  _kernel_object { _irq };
		Signal_context &_context;

	public:

		/**
		 * Construct object that signals interrupt 'irq' via signal 'context'
		 */
		User_irq(unsigned                const  irq,
		         Genode::Irq_session::Trigger   trigger,
		         Genode::Irq_session::Polarity  polarity,
		         Signal_context                &context,
		         Board::Pic                    &pic,
		         Irq::Pool                     &user_irq_pool);

		/**
		 * Destructor
		 */
		~User_irq() { _irq.disable(); }

		/**
		 * Handle occurence of the interrupt
		 */
		void occurred()
		{
			if (_context.can_submit(1)) {
				_context.submit(1);
			}
			_irq.disable();
		}

		static User_irq * object(Irq::Pool &user_irq_pool, unsigned const irq)
		{
			return user_irq_pool.object(irq)->user_irq();
		}

		/**
		 * Syscall to create user irq object
		 *
		 * \param irq       reference to constructible object
		 * \param nr        interrupt number
		 * \param trigger   level or edge
		 * \param polarity  low or high
		 * \param sig       capability of signal context
		 */
		static capid_t syscall_create(Genode::Kernel_object<User_irq> & irq,
		                              unsigned                          nr,
		                              Genode::Irq_session::Trigger      trigger,
		                              Genode::Irq_session::Polarity     polarity,
		                              capid_t                           sig)
		{
			return call(call_id_new_irq(), (Call_arg)&irq, nr,
			            (trigger << 2) | polarity, sig);
		}

		/**
		 * Syscall to delete user irq object
		 *
		 * \param irq  reference to constructible object
		 */
		static void syscall_destroy(Genode::Kernel_object<User_irq> &irq) {
			call(call_id_delete_irq(), (Call_arg) &irq); }

		Object &kernel_object() { return _kernel_object; }

		void enable() { _irq.enable(); }
};

#endif /* _CORE__KERNEL__IRQ_H_ */
