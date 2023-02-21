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
#include <crypto.h>
#include <cbe_librara.h>


void Cbe::Librara::_drop_generated_request(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTO) {
		class Bad_call { };
		throw Bad_call { };
	}
	_lib->librara__drop_generated_request(
		dynamic_cast<Crypto_request *>(&mod_req)->prim());
}


void Cbe::Librara::generated_request_complete(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Bad_call { };
		throw Bad_call { };
	}
	if (mod_req.dst_module_id() != CRYPTO) {
		class Bad_call { };
		throw Bad_call { };
	}
	Crypto_request &req { *dynamic_cast<Crypto_request *>(&mod_req) };
	_lib->librara__generated_request_complete(
		req.prim(), req.result_blk_ptr(), nullptr, nullptr, nullptr,
		req.success());
}
