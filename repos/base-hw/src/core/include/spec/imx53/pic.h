/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
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
#include <spec/imx53/pic_base.h>

namespace Kernel { class Pic : public Imx53::Pic_base { }; }

#endif /* _PIC_H_ */
