/*
 * \brief  Multiprocessing-related CPU implementations
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>

unsigned Kernel::cpu_primary_id() { return 0; }

unsigned Kernel::cpu_executing_id() { return cpu_primary_id(); }
