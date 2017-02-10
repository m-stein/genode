/*
 * \brief  Interface for accessing a timer
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_FILTER__TIMER_ACCESSOR_H_
#define _INPUT_FILTER__TIMER_ACCESSOR_H_

/* Genode includes */
#include <os/timer.h>

namespace Input_filter { struct Timer_accessor; }

struct Input_filter::Timer_accessor { virtual Genode::Timer &timer() = 0; };

#endif /* _INPUT_FILTER__TIMER_ACCESSOR_H_ */
