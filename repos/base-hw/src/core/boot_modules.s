/*
 * \brief  Dummy boot-modules-file for building standalone images of core
 * \author Martin Stein
 * \date   2011-12-16
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
.include "macros.s"

_common_constants

.section .data

.align data_access_alignm_log2
.global _boot_modules_begin
_boot_modules_begin:
.string "GROM"

.align data_access_alignm_log2
.global _boot_module_headers_begin
_boot_module_headers_begin:

/* no module headers */

.global _boot_module_headers_end
_boot_module_headers_end:

/* no modules */

.global _boot_modules_end
_boot_modules_end:
