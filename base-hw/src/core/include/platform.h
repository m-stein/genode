/*
 * \brief  Platform interface
 * \author Martin Stein
 * \date   2011-12-21
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_H_
#define _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_H_

/* Genode includes */
#include <base/sync_allocator.h>
#include <base/allocator_avl.h>
#include <kernel/log.h>
#include <kernel/syscalls.h>

/* Core includes */
#include <platform_generic.h>

namespace Genode {

	/**
	 * Manages platform ressources
	 */
	class Platform : public Platform_generic
	{
		typedef Synchronized_range_allocator<Allocator_avl> Phys_allocator;

		Phys_allocator  _core_mem_alloc; /* Core-accessible memory */
		Phys_allocator  _io_mem_alloc;   /* MMIO allocator         */
		Phys_allocator  _io_port_alloc;  /* I/O port allocator     */
		Phys_allocator  _irq_alloc;      /* IRQ allocator          */
		Rom_fs          _rom_fs;         /* ROM file system        */

		/**
		 * Virtual address range usable by non-core processes
		 */
		addr_t _vm_base;
		size_t _vm_size;

		/**
		 * Get one of the consecutively numbered address regions that
		 * are available at the specific platform
		 *
		 * \return  >0  Region pointer if region with index 'i' exists
		 *          0   If region with index 'i' doesn't exist
		 */
		static Native_region * _ram_regions(unsigned i);
		static Native_region * _mmio_regions(unsigned i);
		static Native_region * _irq_regions(unsigned i);

		/**
		 * Get one of the consecutively numbered address regions that
		 * must be accessable only by core
		 *
		 * \return  >0  Region pointer if region with index 'i' exists
		 *          0   If region with index 'i' doesn't exist
		 */
		static Native_region * _core_only_ram_regions(unsigned i);
		static Native_region * _core_only_mmio_regions(unsigned i);
		static Native_region * _core_only_irq_regions(unsigned i);

		public:

			/**
			 * Constructor
			 */
			Platform();

			/**
			 * We need a 'Range_allocator' instead of 'Allocator' as in
			 * 'Platform_generic' to allocate aligned space for e.g. page tables
			 */
			inline Range_allocator * core_mem_alloc() { return &_core_mem_alloc; }

			inline Range_allocator * ram_alloc() { return &_core_mem_alloc; }

			inline Range_allocator * io_mem_alloc() { return &_io_mem_alloc; }

			inline Range_allocator * io_port_alloc() { return &_io_port_alloc; }

			inline Range_allocator * irq_alloc() { return &_irq_alloc; }

			inline addr_t vm_start() const { return _vm_base; }

			inline size_t vm_size() const { return _vm_size; }

			inline Rom_fs *rom_fs() { return &_rom_fs; }

			inline void wait_for_exit() { while (1) Kernel::pause_thread(); };

			inline Range_allocator * region_alloc()
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
				return 0;
			}
	};
}

#endif /* _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_H_ */

