/*
 * \brief  Interrupt controller for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/cortex_a9/pic_base.h>

namespace Kernel
{
	/**
	 * Interrupt controller for kernel
	 */
	class Pic : public Cortex_a9::Pic_base { };
}


bool Arm_gic::Pic::_use_security_ext() { return 0; }


#endif /* _PIC_H_ */
