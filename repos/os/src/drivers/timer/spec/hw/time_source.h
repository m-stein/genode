/*
 * \brief  Platform-specific timer back end
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
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

		using Microseconds = Genode::Time_source::Microseconds;

		Microseconds mutable _curr_time = 0;
		Microseconds mutable _last_timeout_age = 0;
		Microseconds const   _max_timeout;

	public:

		Time_source(Genode::Entrypoint &ep);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Microseconds max_timeout() const override { return _max_timeout; };
		Microseconds curr_time() const override;
		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler) override;
};

#endif /* _TIME_SOURCE_H_ */
