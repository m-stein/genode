/*
 * \brief  Interrupt controller for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IMX53__PIC_H_
#define _IMX53__PIC_H_

/* core includes */
#include <pic/imx53.h>

namespace Kernel { class Pic : public Imx53::Pic { }; }

#endif /* _IMX53__PIC_H_ */

