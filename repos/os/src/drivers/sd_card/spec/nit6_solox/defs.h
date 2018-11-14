/*
 * \brief  Nitrogen6 SoloX defs for the SDHC driver
 * \author Martin Stein
 * \date   2018-07-19
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEFS_H_
#define _DEFS_H_

/* Genode includes */
#include <drivers/defs/imx6.h>

enum {
	MMIO_BASE = Imx6::SDHC_2_MMIO_BASE,
	IRQ       = Imx6::SDHC_2_IRQ,
};

#endif /* _DEFS_H_ */
