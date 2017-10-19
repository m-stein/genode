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
#include <drivers/defs/imx6.h>

namespace Nit6_solox {

	using namespace Imx6;

	enum {
		/* SD host controller */
		SDHC_IRQ       = 55,
		SDHC_MMIO_BASE = 0x02194000,
		SDHC_MMIO_SIZE = 0x00004000,

		/* normal RAM */
		RAM0_BASE = 0x80000000,
		RAM0_SIZE = 0x40000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__NIT6_SOLOX_H_ */
