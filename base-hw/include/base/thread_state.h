/*
 * \brief  Thread state specific for base-hw
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

/* Genode includes */
#include <base/cpu_context.h>

namespace Genode
{
	struct Thread_state : public Cpu_context { };
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */

