/*
 * \brief  Test for alarm library
 * \author Norman Feske
 * \date   2008-11-05
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>

/* local includes */
#include <timer.h>

using namespace Genode;


class Test_periodic_timeout : public Periodic_timeout<Test_periodic_timeout>
{
	private:

		String<64> _name;
		unsigned   _count = 0;


		/*************
		 ** Timeout **
		 *************/

		void _handle_timeout(Microseconds curr_time)
		{
			_count++;
			log("Timeout ", _name, " triggered ", _count, " times, current time: ", curr_time);
		}

	public:

		Test_periodic_timeout(char              const *name,
		                      Timeout_scheduler       &timer,
		                      Microseconds      const  duration)
		:
			Periodic_timeout(timer, *this, &Test_periodic_timeout::_handle_timeout, duration),
			_name(name)
		{
			log("Timeout ", _name, ": ", (unsigned)duration, " us");
		}
};


class Test_one_shot_timeout : public One_shot_timeout<Test_one_shot_timeout>
{
	private:

		String<64> _name;
		unsigned   _count = 0;


		/*************
		 ** Timeout **
		 *************/

		void _handle_timeout(Microseconds curr_time)
		{
			_count++;
			log("Timeout ", _name, " triggered ", _count, " times, current time: ", curr_time);
		}

	public:

		Test_one_shot_timeout(char              const *name,
		                      Timeout_scheduler       &timer,
		                      Microseconds      const  duration)
		:
			One_shot_timeout(timer, *this, &Test_one_shot_timeout::_handle_timeout),
			_name(name)
		{
			log("Timeout ", _name, ": ", (unsigned)duration, " us");
			trigger(duration);
		}
};


class Main
{
	private:

		Genode::Timer _timer;
		Test_periodic_timeout _periodic_1 { "Periodic_1s",    _timer, 1000000 };
		Test_periodic_timeout _periodic_2 { "Periodic_700ms", _timer, 700000  };
		Test_one_shot_timeout _one_shot_1 { "One_shot_3s",    _timer, 3000000 };
		Test_one_shot_timeout _one_shot_2 { "One_shot_5s",    _timer, 5000000 };

	public:

		Main(Env &env) : _timer(env, env.ep()) { }
};


/***************
 ** Component **
 ***************/

size_t Component::stack_size()        { return 4 * 1024 * sizeof(addr_t); }
void   Component::construct(Env &env) { static Main main(env);            }
