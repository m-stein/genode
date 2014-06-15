/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2014-06-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <board.h>


unsigned Panda::Sysctrl_general_core::max_mpu_clk()
{
	switch (read<Std_fuse_prod_id_1::Silicon_type>()) {
	case 0: return  800000000;
	case 1: return 1000000000;
	case 2: return 1200000000;
	default:
		PWRN("unkown performance type");
		return 0;
	}
}
