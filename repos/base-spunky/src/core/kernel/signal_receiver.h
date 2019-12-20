/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__SIGNAL_RECEIVER_H_
#define _CORE__KERNEL__SIGNAL_RECEIVER_H_


/* Core includes */
#include <kernel/ada_object.h>
#include <kernel/core_interface.h>
#include <object.h>

namespace Kernel
{
	class Thread;
	class Signal_handler;
	class Signal_context_killer;
	class Signal_context;
	class Signal_receiver;
}

struct Kernel::Signal_handler : Ada_object<40>
{
	Signal_handler(Thread &thread);

	~Signal_handler();

	void cancel_waiting();
};

struct Kernel::Signal_context_killer : Ada_object<16>
{
	Signal_context_killer(Thread &thread);

	~Signal_context_killer();

	void cancel_waiting();
};

struct Kernel::Signal_context : Ada_object<88>
{
	Kernel::Object _kernel_object { *this };

	Signal_context(Signal_receiver &r, addr_t const imprint) { initialize(r, imprint); }

	~Signal_context();

	void initialize(Signal_receiver &r, addr_t const imprint);

	void deinitialize();

	bool can_submit(unsigned const n) const;
	void submit(unsigned const n);

	void ack();

	bool can_kill() const;
	void kill(Signal_context_killer &k);

		/**
		 * Create a signal context and assign it to a signal receiver
		 *
		 * \param p         memory donation for the kernel signal-context object
		 * \param receiver  pointer to signal receiver kernel object
		 * \param imprint   user label of the signal context
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(Genode::Kernel_object<Signal_context> &c,
		                              Signal_receiver & receiver,
		                              addr_t const imprint)
		{
			return call(call_id_new_signal_context(), (Call_arg)&c,
			            (Call_arg)&receiver, (Call_arg)imprint);
		}

		/**
		 * Destruct a signal context
		 *
		 * \param context  pointer to signal context kernel object
		 */
		static void syscall_destroy(Genode::Kernel_object<Signal_context> &c) {
			call(call_id_delete_signal_context(), (Call_arg)&c); }

	Object &kernel_object() { return _kernel_object; }
};

struct Kernel::Signal_receiver : Ada_object<48>
{
	Kernel::Object _kernel_object { *this };

	Signal_receiver() { initialize(); }

	~Signal_receiver() { deinitialize(); }

	void initialize();

	void deinitialize();

	bool can_add_handler(Signal_handler const &h) const;
	void add_handler(Signal_handler &h);

		/**
		 * Syscall to create a signal receiver
		 *
		 * \param p  memory donation for the kernel signal-receiver object
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(Genode::Kernel_object<Signal_receiver> &r) {
			return call(call_id_new_signal_receiver(), (Call_arg)&r); }

		/**
		 * Syscall to destruct a signal receiver
		 *
		 * \param receiver  pointer to signal receiver kernel object
		 */
		static void syscall_destroy(Genode::Kernel_object<Signal_receiver> &r) {
			call(call_id_delete_signal_receiver(), (Call_arg)&r); }

	Object &kernel_object() { return _kernel_object; }
};

#endif /* _CORE__KERNEL__SIGNAL_RECEIVER_H_ */
