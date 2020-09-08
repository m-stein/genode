/*
 * \brief  Timer accuracy test
 * \author Norman Feske
 * \author Martin Stein
 * \date   2010-03-09
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/component.h>

using namespace Genode;


struct Main
{
	Timer::Connection    timer;
	Signal_handler<Main> timer_handler;
	uint64_t             duration_us { 0 };

	void handle_timer()
	{
		duration_us += 1000 * 1000;
		timer.trigger_once(duration_us);
		log("");
	}

int xmain (void) __attribute__((aligned(4096))) {
	unsigned volatile x { 2 };
	for (unsigned volatile i = 2; i <= 400000; i++) {
		bool prime_nr { true };
		for (unsigned volatile j = 2; j < i; j++) {
			unsigned volatile k = i / j;
			unsigned volatile l = k * j;
			if (i == l) {
				prime_nr = false;
				break;
			}
		}
		if (prime_nr) {
			x = i;
		}
	}
	return x;
}

	Main(Env &env) : timer(env), timer_handler(env.ep(), *this, &Main::handle_timer)
	{
		while (1) {
			Genode::log("sleep 100 sec");
			timer.msleep(100000);
			Genode::log("measure");
			unsigned long long a = timer.elapsed_us();
			xmain();
			unsigned long long b = timer.elapsed_us();
			Genode::log("result: ", (b-a)/1000, " ms");
		}

		timer.sigh(timer_handler);
		handle_timer();
	}
};


void Component::construct(Env &env) { static Main main(env); }
