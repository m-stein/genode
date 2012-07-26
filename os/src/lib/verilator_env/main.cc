/*
 * \brief   Main routine for the emulation enviroment of verilated designs
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
#include <cap_session/connection.h>

/* local includes */
#include "emulation_root.h"

/* Verilator includes */
#include "verilated.h"

using namespace Genode;

int main(int argc, char **argv, char **env)
{
	/* initialize Verilator enviroment */
	Verilated::commandArgs(argc, argv);

	/* create emulation service */
	enum { ENTRYPOINT_STACK_SIZE = 64 * 1024 };
	static Cap_connection cap;
	static Sliced_heap sliced_heap(Genode::env()->ram_session(),
	                               Genode::env()->rm_session());
	static Rpc_entrypoint emulation_ep(&cap, ENTRYPOINT_STACK_SIZE,
	                                   "wb_emulation_ep");
	static Emulation::Root emulation_root(&emulation_ep, &sliced_heap);

	/* announce emulation service */
	Genode::env()->parent()->announce(emulation_ep.manage(&emulation_root));
	sleep_forever();
}

