/*
 * \brief  Software TLB controls specific for the i.MX53
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__IMX53__TLB_H_
#define _SRC__CORE__IMX53__TLB_H_

/* Genode includes */
#include <arm/v7/section_table.h>
#include <drivers/board.h>

/**
 * Software TLB-controls
 */
class Tlb : public Arm_v7::Section_table
{
	public:

		/**
		 * Placement new
		 */
		void * operator new (Genode::size_t, void * p) { return p; }
};

/**
 * Board specific mapping attributes
 */
struct Page_flags : Arm::Page_flags { };

typedef Arm::page_flags_t page_flags_t;

/**
 * TLB of core
 */
class Core_tlb : public Tlb
{
	public:

		/**
		 * Constructor
		 *
		 * Must ensure that core never gets a pagefault.
		 */
		Core_tlb()
		{
			using namespace Genode;
			map_core_area(Board::CSD0_DDR_RAM_BASE,
			              Board::CSD0_DDR_RAM_SIZE, false);
			map_core_area(Board::MMIO_BASE, Board::MMIO_SIZE, true);
		}
};

#endif /* _SRC__CORE__IMX53__TLB_H_ */

