
/* Genode includes */
#include <base/printf.h>
#include <cap_session/connection.h>

/* local includes */
#include "emulation_root.h"

/* verilator includes */
#include "verilated.h"

using namespace Genode;

int main(int argc, char **argv, char **env)
{
	Genode::printf("--- test verilator runtime enviroment ---\n");

	Verilated::commandArgs(argc, argv);

	/* create emulation service */
	enum { ENTRYPOINT_STACK_SIZE = 4 * 1024 };
	static Cap_connection cap;
	static Sliced_heap sliced_heap(Genode::env()->ram_session(),
	                               Genode::env()->rm_session());
	static Rpc_entrypoint emulation_ep(&cap, ENTRYPOINT_STACK_SIZE,
	                                   "emulation_ep");
	static Emulation::Root emulation_root(&emulation_ep, &sliced_heap);

	/* announce emulation service */
	Genode::env()->parent()->announce(emulation_ep.manage(&emulation_root));
}
