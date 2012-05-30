/*
 * \brief  Dummy version of a boot modules file to enable a 'core' standalone image
 * \author Martin Stein
 * \date   2011-12-16
 */

.section .data

.align 3
.global _boot_modules_begin
_boot_modules_begin:
.string "GROM"

.align 3
.global _boot_module_headers_begin
_boot_module_headers_begin:

/* No module headers */

.global _boot_module_headers_end
_boot_module_headers_end:

/* No modules */

.global _boot_modules_end
_boot_modules_end:
