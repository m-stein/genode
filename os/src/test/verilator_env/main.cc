/*
 * \brief   Test device that is in fact an emulated verilog design
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <io_mem_session/connection.h>

using namespace Genode;

/**
 * Main routine
 */
int main(int argc, char **argv)
{
	printf("--- test device that is in fact an emulated verilog design ---\n");
	enum {
		MM_ROM_MMIO_BASE = 0x71000000,
		MM_ROM_MMIO_SIZE = 0x1000,
	};
	static Rm_session * const rm = env()->rm_session();
	static Io_mem_connection mm_rom_mmio(MM_ROM_MMIO_BASE, MM_ROM_MMIO_SIZE);
	unsigned * a = (unsigned *)rm->attach(mm_rom_mmio.dataspace());
	printf("Lese 0x0: %x\n", a[0]);
	printf("Lese 0x4: %x\n", a[1]);
	printf("Lese 0x8: %x\n", a[2]);
	printf("Lese 0x10: %x\n", a[2]);
	printf("Lese 0x14: %x\n", a[2]);
	while(1);
}

