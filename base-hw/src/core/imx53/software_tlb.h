/*
 * \brief  Software TLB controls specific for the i.MX53
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__IMX53__SOFTWARE_TLB_H_
#define _SRC__CORE__IMX53__SOFTWARE_TLB_H_

/* Genode includes */
#include <arm/v7/section_table.h>

/**
 * Software TLB controls
 */
class Software_tlb : public Arm_v7::Section_table { };

#endif /* _SRC__CORE__IMX53__SOFTWARE_TLB_H_ */

