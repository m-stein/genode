/*
 * \brief  Abstract time-source interface
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__TIME_SOURCE_H_
#define _OS__TIME_SOURCE_H_

#include <base/output.h>

namespace Genode { class Time_source; }


struct Genode::Time_source
{
	struct Microseconds
	{
		unsigned long value;

		explicit Microseconds(unsigned long const value) : value(value) { }

		void print(Output &output) const { Genode::print(output, value); }
	};

	struct Timeout_handler
	{
		virtual void handle_timeout(Microseconds curr_time) = 0;
	};

	virtual Microseconds curr_time() const = 0;

	virtual Microseconds max_timeout() const = 0;

	virtual void schedule_timeout(Microseconds duration, Timeout_handler &handler) = 0;
};

#endif /* _OS__TIME_SOURCE_H_ */
