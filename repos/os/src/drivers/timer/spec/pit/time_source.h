/*
 * \brief  Platform timer based on the Programmable Interval Timer (PIT)
 * \author Norman Feske
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIME_SOURCE_H_
#define _TIME_SOURCE_H_

/* Genode includes */
#include <io_port_session/connection.h>
#include <irq_session/connection.h>

/* local includes */
#include <signalled_time_source.h>

namespace Timer { class Time_source; }


class Timer::Time_source : public Genode::Signalled_time_source
{
	private:

		Genode::Io_port_connection _io_port;
		Genode::Irq_connection     _timer_irq;
		Microseconds     mutable   _curr_time;
		Genode::uint16_t mutable   _counter_init_value;
		bool             mutable   _handled_wrap;

		void _set_counter(Genode::uint16_t value);

		Genode::uint16_t _read_counter(bool *wrapped);

	public:

		Time_source(Genode::Entrypoint &ep);


		/*************************
		 ** Genode::Time_source **
		 *************************/

		Microseconds curr_time() const override;
		Microseconds max_timeout() const override;
		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler) override;
};

#endif /* _TIME_SOURCE_H_ */
