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
#include <cbe_init_librara.h>
#include <trust_anchor.h>

using namespace Genode;
using namespace Cbe;


void Cbe_init::Librara::_drop_generated_request(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib.librara__drop_generated_request(req.prim_ptr());
		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Cbe_init::Librara::generated_request_complete(Module_request &mod_req)
{
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib.librara__generated_request_complete(
			req.prim_ptr(), req.key_plaintext_ptr(), req.key_ciphertext_ptr(),
			req.success());

		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}
