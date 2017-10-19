/*
 * \brief  MMIO and IRQ definitions of the Wandboard Quad
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2016 Ksys Labs LLC
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__WAND_QUAD_H_
#define _INCLUDE__DRIVERS__DEFS__WAND_QUAD_H_

/* Genode includes */
#include <drivers/defs/imx6.h>

namespace Wand_quad {

	using namespace Imx6;

	enum {
		/* SD host controller */
		SDHC_IRQ       = 54,
		SDHC_MMIO_BASE = 0x02190000,
		SDHC_MMIO_SIZE = 0x00004000,

		/* normal RAM */
		RAM0_BASE = 0x10000000,
		RAM0_SIZE = 0x80000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__WAND_QUAD_H_ */
