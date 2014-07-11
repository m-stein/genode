/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \date   2013-01-22
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__CORELINK_GIC400__PIC_SUPPORT_H_
#define _SPEC__CORELINK_GIC400__PIC_SUPPORT_H_

/* core includes */
#include <spec/arm_gic/pic_support.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Corelink_gic400;
}

class Genode::Corelink_gic400 : public Arm_gic
{
	enum {
		DISTR_OFFSET = 0x1000,
		CPU_OFFSET   = 0x2000,
	};

	public:

		/**
		 * Constructor
		 */
		Corelink_gic400(addr_t const base)
		:
			Arm_gic(base + DISTR_OFFSET, base + CPU_OFFSET)
		{ }
};


bool Genode::Arm_gic::_use_security_ext() { return 0; }


#endif /* _SPEC__CORELINK_GIC400__PIC_BASE_H_ */
