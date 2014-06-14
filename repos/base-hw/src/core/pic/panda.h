/*
 * \brief  Interrupt-controller driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC__PANDA_H_
#define _PIC__PANDA_H_

/* core includes */
#include <pic/cortex_a9.h>

bool Arm_gic::Pic::_use_security_ext() { return 0; }

#endif /* _PIC__PANDA_H_ */
