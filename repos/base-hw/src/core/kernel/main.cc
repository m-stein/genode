/*
 * \brief  Main object of the kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2021-07-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <util/reconstructible.h>

/* base-hw Core includes */
#include <kernel/cpu.h>
#include <kernel/lock.h>
#include <kernel/main.h>

/* base-hw-internal includes */
#include <hw/boot_info.h>


namespace Kernel {

	class Main;
}


class Kernel::Main
{
	private:

		static Main *_instance;

		Lock                                    _data_lock        { };
		Cpu_pool                                _cpu_pool         { };
		Genode::Constructible<Core_main_thread> _core_main_thread { };

		void _handle_kernel_entry();

	public:

		static void handle_kernel_entry();

		static void initialize_and_handle_kernel_entry();

		static time_t read_idle_thread_execution_time(unsigned cpu_idx);
};


Kernel::Main *Kernel::Main::_instance;


void Kernel::Main::_handle_kernel_entry()
{
	Cpu &cpu = _cpu_pool.cpu(Cpu::executing_id());
	Cpu_job * new_job;

	{
		Lock::Guard guard(_data_lock);

		new_job = &cpu.schedule();
	}

	new_job->proceed(cpu);
}


void Kernel::Main::handle_kernel_entry()
{
	_instance->_handle_kernel_entry();
}


void Kernel::Main::initialize_and_handle_kernel_entry()
{
	static volatile bool instance_initialized = false;
	static volatile bool pool_ready           = false;
	static volatile bool kernel_ready         = false;

	bool const primary_cpu { Cpu::executing_id() == Cpu::primary_id() };

	if (primary_cpu) {

		/**
		 * Let the primary CPU create a Main object and initialize the static
		 * reference to it.
		 */
		static Main instance { };
		_instance = &instance;

	} else {

		/**
		 * Let secondary CPUs block until the primary CPU has managed to set
		 * up the Main instance.
		 */
		while (!instance_initialized) { }
	}

	{
		Lock::Guard guard(_instance->_data_lock);

		instance_initialized = true;

		/* initialize current cpu */
		pool_ready = _instance->_cpu_pool.initialize();
	};

	/* wait until all cpus have initialized their corresponding cpu object */
	while (!pool_ready) { ; }

	/* the boot-cpu initializes the rest of the kernel */
	if (primary_cpu) {
		Lock::Guard guard(_instance->_data_lock);

		using Boot_info = Hw::Boot_info<Board::Boot_info>;
		Boot_info &boot_info {
			*reinterpret_cast<Boot_info*>(Hw::Mm::boot_info().base) };

		_instance->_cpu_pool.for_each_cpu([&] (Kernel::Cpu &cpu) {
			boot_info.kernel_irqs.add(cpu.timer().interrupt_id());
		});
		boot_info.kernel_irqs.add((unsigned)Board::Pic::IPI);

		Genode::log("");
		Genode::log("kernel initialized");

		_instance->_core_main_thread.construct(_instance->_cpu_pool);
		boot_info.core_main_thread_utcb =
			(addr_t)_instance->_core_main_thread->utcb();

		kernel_ready = true;
	} else {
		/* secondary cpus spin until the kernel is initialized */
		while (!kernel_ready) {;}
	}

	_instance->_handle_kernel_entry();
}


Kernel::time_t Kernel::Main::read_idle_thread_execution_time(unsigned cpu_idx)
{
	return _instance->_cpu_pool.cpu(cpu_idx).idle_thread().execution_time();
}


Kernel::time_t Kernel::main_read_idle_thread_execution_time(unsigned cpu_idx)
{
	return Main::read_idle_thread_execution_time(cpu_idx);
}


void Kernel::main_handle_kernel_entry()
{
	Main::handle_kernel_entry();
}
