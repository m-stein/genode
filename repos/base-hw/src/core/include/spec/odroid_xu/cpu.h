/*
 * \brief  CPU driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/cortex_a15/cpu_support.h>
#include <spec/uniprocessor/cpu_support.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Cpu : public Cortex_a15, public Uniprocessor { };
}

namespace Kernel { using Genode::Cpu; }

#endif /* _CPU_H_ */
