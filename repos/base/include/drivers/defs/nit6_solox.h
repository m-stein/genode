/*
 * \brief  MMIO and IRQ definitions for Nit6 SOLOX board
 * \author Stefan Kalkowski
 * \date   2017-10-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__NIT6_SOLOX_H_
#define _INCLUDE__DRIVERS__DEFS__NIT6_SOLOX_H_

/* Genode includes */
#include <drivers/defs/imx6_solox.h>

namespace Nit6_solox {

	using namespace Imx6_solox;

	enum {
		/* normal RAM */
		RAM0_BASE = 0x80000000,
		RAM0_SIZE = 0x40000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__NIT6_SOLOX_H_ */
