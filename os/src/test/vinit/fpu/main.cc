/*
 * \brief  FPU emulator
 * \author Martin stein
 * \date   2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <cap_session/connection.h>

/* local includes */
#include <emulation_root.h>

using namespace Genode;


/**
 * Main routine
 */
int main(int argc, char **argv)
{
	printf("--- FPU emulator ---\n");

	/* create emulation service */
	enum { ENTRYPOINT_STACK_SIZE = 4 * 1024 };
	static Cap_connection cap;
	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());
	static Rpc_entrypoint emulation_ep(&cap, ENTRYPOINT_STACK_SIZE,
	                                   "emulation_ep");
	static Emulation::Root emulation_root(&emulation_ep, &sliced_heap);

	/* announce emulation service and go to sleep */
	env()->parent()->announce(emulation_ep.manage(&emulation_root));
	sleep_forever();
	return 0;
}

