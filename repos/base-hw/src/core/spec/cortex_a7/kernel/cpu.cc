/*
 * \brief   Cpu class implementation specific to Cortex A7 SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <pic.h>
#include <board.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

/* entrypoint for non-boot CPUs */
extern "C" void * _start_secondary_cpus;

/* indicates boot cpu status */
static volatile bool primary_cpu = true;

void Kernel::Cpu_context::_init(unsigned int, unsigned long) {PDBG("implement me");}
