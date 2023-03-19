/*
 * \brief  Temporary module compliant wrapper for the CBE library
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <superblock_control.h>
#include <cbe_librara.h>


void Cbe::Librara::_drop_generated_request(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case SUPERBLOCK_CONTROL:
	{
		Superblock_control_request &req {
			*dynamic_cast<Superblock_control_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Cbe::Librara::generated_request_complete(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case SUPERBLOCK_CONTROL:
	{
		Superblock_control_request &req {
			*dynamic_cast<Superblock_control_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), req.sb_state(), req.success());

		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}
