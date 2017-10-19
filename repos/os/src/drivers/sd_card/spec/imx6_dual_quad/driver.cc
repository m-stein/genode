/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

/* Genode includes */
#include <drivers/defs/imx6_dual_quad.h>

using namespace Sd_card;
using namespace Genode;


Driver::Driver(Env &env)
:
	Driver_base(env.ram()),
	Attached_mmio(env, Imx6_dual_quad::SDHC_MMIO_BASE, Imx6_dual_quad::SDHC_MMIO_SIZE),
	_env(env), _irq(env, Imx6_dual_quad::SDHC_IRQ)
{
	log("SD card detected");
	log("capacity: ", card_info().capacity_mb(), " MiB");
}
