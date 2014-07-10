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

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/corelink_gic400/pic_base.h>
#include <board.h>

namespace Genode
{
	/**
	 * Interrupt controller for kernel
	 */
	class Pic : public Corelink_gic400::Pic_base
	{
		public:

			/**
			 * Constructor
			 */
			Pic() : Corelink_gic400::Pic_base(Board::GIC_CPU_MMIO_BASE) { }
	};
}


bool Arm_gic::Pic::_use_security_ext() { return 0; }


namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
