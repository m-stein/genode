/*
 * \brief  Time source that uses the timeout syscalls of the HW kernel
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* local includes */
#include <signalled_time_source.h>

namespace Timer { class Time_source; }


class Timer::Time_source : public Genode::Signalled_time_source
{
	private:

		unsigned long mutable _curr_time_us = 0;
		unsigned long mutable _last_timeout_age_us = 0;
		unsigned long const   _max_timeout_us;

	public:

		Time_source(Genode::Env &env);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Microseconds curr_time() const override;
		void schedule_timeout(Microseconds duration, Timeout_handler &handler) override;
		Microseconds max_timeout() const override {
			return Microseconds(_max_timeout_us); };
};

#endif /* _TIME_SOURCE_H_ */
