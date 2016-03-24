/*
 * \brief   Test the performance of Core and the underlying Kernel
 * \author  Martin Stein
 * \date    2016-03-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <dummy_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

int main()
{
	enum { ROUNDS = 60000 };

	Dummy::Connection         dummy;
	Timer::Connection         timer;
	Signal_receiver           sigrec;
	Signal_context            sigctx;
	Signal_context_capability sigcap = sigrec.manage(&sigctx);

	timer.sigh(sigcap);
	for (int i = 0; i < ROUNDS; i++) {
		int ret = dummy.dummy_rpc(i);
		if (ret != ~i) { PERR("bad RPC response"); }
		timer.trigger_once(1);
		sigrec.wait_for_signal();
	}
	printf("done\n");
	sleep_forever();
}
