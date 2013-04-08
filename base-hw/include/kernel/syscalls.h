/*
 * \brief  Kernels syscall frontend
 * \author Martin stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__KERNEL__SYSCALLS_H_
#define _INCLUDE__KERNEL__SYSCALLS_H_

/* Genode includes */
#include <base/syscall_types.h>

namespace Genode
{
	class Platform_thread;
	class Platform_pd;
	class Tlb;
}

namespace Kernel
{
	typedef Genode::Tlb             Tlb;
	typedef Genode::addr_t          addr_t;
	typedef Genode::size_t          size_t;
	typedef Genode::Platform_thread Platform_thread;
	typedef Genode::Platform_pd     Platform_pd;

	/**
	 * Unique opcodes of all syscalls supported by the kernel
	 */
	enum Syscall_type
	{
		INVALID_SYSCALL = 0,

		/* execution control */
		NEW_THREAD = 1,
		DELETE_THREAD = 26,
		START_THREAD = 2,
		PAUSE_THREAD = 3,
		RESUME_THREAD = 4,
		RESUME_FAULTER = 28,
		GET_THREAD = 5,
		CURRENT_THREAD_ID = 6,
		YIELD_THREAD = 7,
		READ_THREAD_STATE = 18,
		WRITE_THREAD_STATE = 19,

		/* interprocess communication */
		REQUEST_AND_WAIT = 8,
		REPLY = 9,
		WAIT_FOR_REQUEST = 10,

		/* management of resource protection-domains */
		SET_PAGER = 11,
		UPDATE_PD = 12,
		NEW_PD = 13,

		/* interrupt handling */
		ALLOCATE_IRQ = 14,
		AWAIT_IRQ = 15,
		FREE_IRQ = 16,

		/* debugging */
		PRINT_CHAR = 17,

		/* asynchronous signalling */
		NEW_SIGNAL_RECEIVER = 20,
		NEW_SIGNAL_CONTEXT = 21,
		KILL_SIGNAL_CONTEXT = 30,
		AWAIT_SIGNAL = 22,
		SUBMIT_SIGNAL = 23,
		SIGNAL_PENDING = 27,
		ACK_SIGNAL = 29,

		/* vm specific */
		NEW_VM = 24,
		RUN_VM = 25,
	};

	/*****************************************************************
	 ** Syscall with 1 to 6 arguments                               **
	 **                                                             **
	 ** These functions must not be inline to ensure that objects,  **
	 ** wich are referenced by arguments, are tagged as "used" even **
	 ** though only the pointer gets handled in here.               **
	 *****************************************************************/

	Syscall_ret syscall(Syscall_arg arg_0);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3,
	                    Syscall_arg arg_4);

	Syscall_ret syscall(Syscall_arg arg_0,
	                    Syscall_arg arg_1,
	                    Syscall_arg arg_2,
	                    Syscall_arg arg_3,
	                    Syscall_arg arg_4,
	                    Syscall_arg arg_5);

	/**
	 * Virtual range of the mode transition region in every PD
	 */
	addr_t mode_transition_virt_base();
	size_t mode_transition_size();

	/**
	 * Get sizes of the kernel objects
	 */
	size_t thread_size();
	size_t pd_size();
	size_t signal_context_size();
	size_t signal_receiver_size();
	size_t vm_size();

	/**
	 * Get alignment constraints of the kernel objects
	 */
	unsigned kernel_pd_alignm_log2();


	/**
	 * Create a new PD
	 *
	 * \param dst  physical base of an appropriate portion of memory
	 *             that is thereupon allocated to the kernel
	 * \param pd   core local Platform_pd object
	 *
	 * \retval >0  ID of the new PD
	 * \retval  0  if no new PD was created
	 *
	 * Restricted to core threads. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline int new_pd(void * const dst, Platform_pd * const pd) {
		return syscall(NEW_PD, (Syscall_arg)dst, (Syscall_arg)pd); }


	/**
	 * Propagate changes in PD configuration
	 *
	 * \param pd_id  ID of the PD that has been configured
	 *
	 * It might be, that the kernel and/or the hardware caches parts of PD
	 * configurations such as virtual address translations. This syscall
	 * ensures that the current configuration of the targeted PD gets fully
	 * applied from the moment it returns to the userland. This syscall is
	 * inappropriate in case that a PD wants to change its own configuration.
	 * There's no need for this syscall after a configuration change that
	 * can't affect the kernel and/or hardware caches.
	 *
	 * Restricted to core threads.
	 */
	inline void update_pd(unsigned const pd_id) {
		syscall(UPDATE_PD, (Syscall_arg)pd_id); }


	/**
	 * Create a new thread that is stopped initially
	 *
	 * \param dst  physical base of an appropriate portion of memory
	 *             that is thereupon allocated to the kernel
	 * \param pt   assigned platform thread
	 *
	 * \retval >0  ID of the new thread
	 * \retval  0  if no new thread was created
	 *
	 * Restricted to core threads. Regaining of the supplied memory can be done
	 * through 'delete_thread'.
	 */
	inline int
	new_thread(void * const dst, Platform_thread * const pt) {
		return syscall(NEW_THREAD, (Syscall_arg)dst, (Syscall_arg)pt); }

	/**
	 * Delete an existing thread
	 *
	 * \param id  kernel name of the targeted thread
	 *
	 * Restricted to core threads. After calling this, the memory that was
	 * granted beforehand by 'new_thread' to kernel for managing this thread
	 * is freed again.
	 */
	inline void delete_thread(unsigned thread_id) {
		syscall(DELETE_THREAD, (Syscall_arg)thread_id); }

	/**
	 * Start thread with a given context and let it participate in CPU scheduling
	 *
	 * \param id  ID of targeted thread
	 * \param ip  initial instruction pointer
	 * \param sp  initial stack pointer
	 *
	 * \retval >0  success, return value is the TLB of the thread
	 * \retval  0  the targeted thread wasn't started or was already started
	 *             when this gets called (in both cases it remains untouched)
	 *
	 * Restricted to core threads.
	 */
	inline Tlb * start_thread(Platform_thread * const phys_pt, void * ip,
	                          void * sp, unsigned cpu_no)
	{
		return (Tlb *)syscall(START_THREAD, (Syscall_arg)phys_pt,
		                              (Syscall_arg)ip, (Syscall_arg)sp,
		                              (Syscall_arg)cpu_no);
	}


	/**
	 * Prevent thread from participating in CPU scheduling
	 *
	 * \param id  ID of the targeted thread. If not set
	 *            this will target the current thread.
	 *
	 * \retval  0  syscall was successful
	 * \retval <0  if the targeted thread does not exist or still participates
	 *             in CPU scheduling after
	 *
	 * If the caller doesn't target itself, this is restricted to core threads.
	 */
	inline int pause_thread(unsigned const id = 0) {
		return syscall(PAUSE_THREAD, id); }


	/**
	 * Let an already started thread participate in CPU scheduling
	 *
	 * \param id  ID of the targeted thread
	 *
	 * \retval  0  if syscall was successful and thread were paused beforehand
	 * \retval >0  if syscall was successful and thread were already active
	 * \retval <0  if targeted thread doesn't participate in CPU
	 *             scheduling after
	 */
	inline int resume_thread(unsigned const id = 0) {
		return syscall(RESUME_THREAD, id); }


	/**
	 * Continue thread after a pagefault that could be resolved
	 *
	 * \param id  ID of the targeted thread
	 */
	inline void resume_faulter(unsigned const id = 0) {
		syscall(RESUME_FAULTER, id); }


	/**
	 * Let the current thread give up its remaining timeslice
	 *
	 * \param id  if this thread ID is set and valid this will resume the
	 *            targeted thread additionally
	 */
	inline void yield_thread(unsigned const id = 0) {
		syscall(YIELD_THREAD, id); }


	/**
	 * Get the thread ID of the current thread
	 */
	inline int current_thread_id() { return syscall(CURRENT_THREAD_ID); }


	/**
	 * Get platform thread by ID or 0 if target is "core main" or "idle"
	 *
	 * \param id  ID of the targeted thread or 0 if caller targets itself
	 *
	 * Restricted to core threads.
	 */
	inline Platform_thread * get_thread(unsigned const id = 0) {
		return (Platform_thread *)syscall(GET_THREAD, id); }


	/**
	 * Send IPC request and wait for reply
	 *
	 * \param id    ID of the receiver thread
	 * \param size  request size (beginning with the callers UTCB base)
	 *
	 * \return  size of received reply (beginning with the callers UTCB base)
	 *
	 * If the receiver exists, this blocks execution until a dedicated reply
	 * message has been send by the receiver. The receiver may never do so.
	 */
	inline size_t request_and_wait(unsigned const id, size_t const size) {
		return (size_t)syscall(REQUEST_AND_WAIT, id, size); }


	/**
	 * Wait for next IPC request, discard current request
	 *
	 * \return  size of received request (beginning with the callers UTCB base)
	 */
	inline size_t wait_for_request() {
		return (size_t)syscall(WAIT_FOR_REQUEST); }


	/**
	 * Reply to last IPC request
	 *
	 * \param size           reply size (beginning with the callers UTCB base)
	 * \param await_request  if the call shall await and fetch next request
	 *
	 * \return  request size (beginning with the callers UTCB base)
	 *          if await_request was set
	 */
	inline size_t reply(size_t const size, bool const await_request) {
		return (size_t)syscall(REPLY, size, await_request); }


	/**
	 * Set a thread that gets informed about pagefaults of another thread
	 *
	 * \param pager_id    ID of the thread that shall get informed.
	 *                    Subsequently this thread gets an IPC message,
	 *                    wich contains an according 'Pagefault' object for
	 *                    every pagefault the faulter throws.
	 * \param faulter_id  ID of the thread that throws the pagefaults
	 *                    wich shall be notified. After every pagefault this
	 *                    thread remains paused to be reactivated by
	 *                    'resume_thread'.
	 *
	 * Restricted to core threads.
	 */
	inline void set_pager(unsigned const pager_id, unsigned const faulter_id) {
		syscall(SET_PAGER, pager_id, faulter_id); }

	/**
	 * Print a char 'c' to the kernels serial ouput
	 */
	inline void print_char(char const c)
	{ syscall(PRINT_CHAR, (Syscall_arg)c); }

	/**
	 * Allocate an IRQ to the caller if the IRQ is not allocated already
	 *
	 * \param id  ID of the targeted IRQ
	 *
	 * \return  wether the IRQ has been allocated to this thread or not
	 *
	 * Restricted to core threads.
	 */
	inline bool allocate_irq(unsigned const id) {
		return syscall(ALLOCATE_IRQ, (Syscall_arg)id); }


	/**
	 * Free an IRQ from allocation if it is allocated by the caller
	 *
	 * \param id  ID of the targeted IRQ
	 *
	 * \return  wether the IRQ has been freed or not
	 *
	 * Restricted to core threads.
	 */
	inline bool free_irq(unsigned const id)	{
		return syscall(FREE_IRQ, (Syscall_arg)id); }


	/**
	 * Block caller for the occurence of its IRQ
	 *
	 * Restricted to core threads. Blocks the caller forever
	 * if he has not allocated any IRQ.
	 */
	inline void await_irq() { syscall(AWAIT_IRQ); }


	/**
	 * Copy the current state of a thread to the callers UTCB
	 *
	 * \param thread_id  ID of the targeted thread
	 *
	 * Restricted to core threads. One can also read from its own context,
	 * or any thread that is active in the meantime. In these cases
	 * be aware of the fact, that the result reflects the thread
	 * state that were backed at the last kernel entry of the thread.
	 * The copy might be incoherent when this function returns because
	 * the caller might get scheduled away before then.
	 */
	inline void read_thread_state(unsigned const thread_id) {
		syscall(READ_THREAD_STATE, (Syscall_arg)thread_id); }


	/**
	 * Override the state of a thread with the callers UTCB content
	 *
	 * \param thread_id  ID of the targeted thread
	 *
	 * Restricted to core threads. One can also write to its own context, or
	 * to that of a thread that is active in the meantime.
	 */
	inline void write_thread_state(unsigned const thread_id) {
		syscall(WRITE_THREAD_STATE, (Syscall_arg)thread_id); }


	/**
	 * Create a kernel object that acts as receiver for asynchronous signals
	 *
	 * \param dst  physical base of an appropriate portion of memory
	 *             that is thereupon allocated to the kernel
	 *
	 * \return  ID of the new kernel object
	 *
	 * Restricted to core threads. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline unsigned new_signal_receiver(void * dst) {
		return syscall(NEW_SIGNAL_RECEIVER, (Syscall_arg)dst); }


	/**
	 * Create a kernel object that acts as a distinct signal type at a receiver
	 *
	 * \param dst          physical base of an appropriate portion of memory
	 *                     that is thereupon allocated to the kernel
	 * \param receiver_id  ID of the receiver kernel-object that shall
	 *                     provide the new signal context
	 * \param imprint      Every signal, one receives at the new context,
	 *                     will hold this imprint. This enables the receiver
	 *                     to interrelate signals with the context.
	 *
	 * \return  ID of the new kernel object
	 *
	 * Core-only syscall. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline unsigned new_signal_context(void * dst, unsigned receiver_id,
	                                   unsigned imprint)
	{
		return syscall(NEW_SIGNAL_CONTEXT, (Syscall_arg)dst,
		               (Syscall_arg)receiver_id, (Syscall_arg)imprint);
	}


	/**
	 * Wait for occurence of at least one signal at any context of a receiver
	 *
	 * \param receiver_id  ID of the targeted receiver kernel-object
	 *
	 * When this call returns, an instance of 'Signal::Data' is located at the
	 * base of the callers UTCB. It's granted that every occurence of a signal
	 * is provided through this function, exactly till it gets delivered through
	 * this function. If multiple threads listen at the same receiver, and/or
	 * multiple contexts of the receiver trigger simultanously, there is no
	 * assertion about wich thread receives, and from wich context. But
	 * deliveries belonging to the same context are serialized through
	 * 'ack_signal', to enable synchronization in 'kill_signal'.
	 */
	inline void await_signal(unsigned receiver_id) {
		syscall(AWAIT_SIGNAL, (Syscall_arg)receiver_id); }


	/**
	 * Get summarized state of all contexts of a signal receiver
	 *
	 * \param receiver_id  ID of the targeted receiver kernel-object
	 */
	inline bool signal_pending(unsigned receiver_id) {
		return syscall(SIGNAL_PENDING, (Syscall_arg)receiver_id); }


	/**
	 * Trigger a specific signal context
	 *
	 * \param context_id  ID of the targeted context kernel-object
	 * \param num         how often the context shall be triggered by this call
	 */
	inline void submit_signal(unsigned context_id, int num) {
		syscall(SUBMIT_SIGNAL, (Syscall_arg)context_id, (Syscall_arg)num); }

	/**
	 * Acknowledge the processing of the last signal of a signal context
	 *
	 * \param context_id  kernel name of the targeted signal context
	 *
	 * Should be called after all signal objects, that reference the targeted
	 * signal context in userland are destructed. The signal context wont
	 * deliver a new signal until the old signal is acknowledged.
	 */
	inline void ack_signal(unsigned context_id) {
		syscall(ACK_SIGNAL, (Syscall_arg)context_id); }

	/**
	 * Destruct a signal context
	 *
	 * \param context_id  kernel name of the targeted signal context
	 *
	 * Blocks the caller until the last delivered signal of the targeted
	 * context is acknowledged. Then the context gets destructed, losing
	 * all submits that were not delivered when this syscall occured.
	 */
	inline void kill_signal_context(unsigned context_id) {
		syscall(KILL_SIGNAL_CONTEXT, (Syscall_arg)context_id); }

	/**
	 * Create a new virtual-machine that is stopped initially
	 *
	 * \param dst         physical base of an appropriate portion of memory
	 *                    that is thereupon allocated to the kernel
	 * \param state       location of the CPU state of the VM
	 * \param context_id  ID of the targeted signal context
	 *
	 * \retval >0  ID of the new VM
	 * \retval  0  if no new VM was created
	 *
	 * Restricted to core threads. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline int new_vm(void * const dst, void * const state,
	                  unsigned context_id)
	{
		return syscall(NEW_VM, (Syscall_arg)dst, (Syscall_arg)state,
		               (Syscall_arg)context_id);
	}


	/**
	 * Execute a virtual-machine (again)
	 *
	 * \param id  ID of the targeted VM
	 *
	 * Restricted to core threads.
	 */
	inline void run_vm(unsigned const id) {
		syscall(RUN_VM, (Syscall_arg)id); }
}

#endif /* _INCLUDE__KERNEL__SYSCALLS_H_ */

