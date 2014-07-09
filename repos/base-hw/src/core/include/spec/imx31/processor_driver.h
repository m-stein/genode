/*
 * \brief  Processor driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER_H_
#define _PROCESSOR_DRIVER_H_

/* core includes */
#include <processor_driver/arm_v6.h>

namespace Genode
{
	using Arm_v6::Processor_lazy_state;
	using Arm_v6::Processor_driver;
}

#endif /* _PROCESSOR_DRIVER_H_ */
