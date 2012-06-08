/*
 * \brief  32-bit-adder-emulator
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
#include <kernel/log.h>

/* Local includes */
#include "adder32_root.h"

using namespace Genode;

enum { STACK_SIZE = 8*1024 };


/**
 * Main routine
 */
int main(int argc, char **argv)
{
	printf("--- emulator for a 32-bit adder ---\n");

	/*  Initialize ADDER32 service */
	static Cap_connection cap;
	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());
	static Rpc_entrypoint adder32_ep(&cap, STACK_SIZE, "adder32_ep");
	static Adder32_root adder32_root(&adder32_ep, &sliced_heap);

	/* Announce ADDER32 service and go to sleep */
	env()->parent()->announce(adder32_ep.manage(&adder32_root));
	sleep_forever();
	return 0;
}

