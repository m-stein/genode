/*
 * \brief  Try to DoS-attack core through its capability-reference-counter limit
 * \author Martin Stein
 * \date   2016-11-15
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <base/component.h>
#include <base/signal.h>
#include <base/log.h>
#include <util/volatile_object.h>

using namespace Genode;

enum { NR_OF_CAP_REFS = 500 };


struct Test_signal
{
	Signal_context            context;
	Signal_receiver           receiver;
	Signal_context_capability handler { receiver.manage(&context) };
};


struct Cpu_test
{
	Test_signal             sig;
	Cpu_session::Name const name { "foo" };

	Cpu_test(Env &env);
};


Cpu_test::Cpu_test(Env &env)
{
	unsigned cnt = 0;
	try {
		for(; cnt < NR_OF_CAP_REFS; cnt++) {
			env.cpu().create_thread(env.pd_session_cap(), name,
			                        Affinity::Location(), Cpu_session::Weight());
		}
		log("Threads don't trigger");
	}
	catch (...) { log("Threads trigger after ", cnt); }
	try {
		env.cpu().exception_sigh(sig.handler);
		log("CPU exception handlers don't trigger");
	}
	catch (...) { log("CPU exception handlers trigger"); }
}


struct Region_map_test
{
	struct Region_map
	{
		Rm_connection     connection;
		Region_map_client client;

		Region_map(Env &env) : connection(env), client(connection.create(1)) { }
	};

	Test_signal                      sig;
	Lazy_volatile_object<Region_map> rm[NR_OF_CAP_REFS];

	Region_map_test(Env &env);
};


Region_map_test::Region_map_test(Env &env)
{
	unsigned cnt = 0;
	try {
		for(; cnt < NR_OF_CAP_REFS; cnt++) {
			rm[cnt].construct(env);
			rm[cnt]->client.fault_handler(sig.handler); }
		log("RM fault handlers don't trigger");
	}
	catch (...) { log("RM fault handlers trigger after ", cnt); }
}


struct Main
{
	Cpu_test        cpu_test;
	Region_map_test rm_test;

	Main(Env &env);
};


Main::Main(Env &env)
: cpu_test(env), rm_test(env) { log("Test done"); }


/***************
 ** Component **
 ***************/

size_t Component::stack_size()        { return 4 * 1024 * sizeof(addr_t); }
void   Component::construct(Env &env) { static Main main(env); }
