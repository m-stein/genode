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
#include <trust_anchor.h>
#include <block_io.h>
#include <cbe_librara.h>


void Cbe::Librara::_drop_generated_request(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	switch (mod_req.dst_module_id()) {
	case CRYPTO:
	{
		Crypto_request &req {
			*dynamic_cast<Crypto_request *>(&mod_req) };

		_lib->librara__drop_generated_request(req.prim_ptr());
		break;
	}
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib->librara__drop_generated_request(req.prim_ptr());
		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &req {
			*dynamic_cast<Block_io_request *>(&mod_req) };

		_lib->librara__drop_generated_request(req.prim_ptr());
		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Cbe::Librara::generated_request_complete(Module_request &mod_req)
{
	if (!_lib.constructed()) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	switch (mod_req.dst_module_id()) {
	case CRYPTO:
	{
		Crypto_request &req {
			*dynamic_cast<Crypto_request *>(&mod_req) };

		_lib->librara__generated_request_complete(
			req.prim_ptr(), req.result_blk_ptr(), nullptr, nullptr, nullptr,
			req.success());

		break;
	}
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &req {
			*dynamic_cast<Trust_anchor_request *>(&mod_req) };

		_lib->librara__generated_request_complete(
			req.prim_ptr(), nullptr, req.key_plaintext_ptr(),
			req.key_ciphertext_ptr(), req.hash_ptr(),
			req.success());

		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &req {
			*dynamic_cast<Block_io_request *>(&mod_req) };

		_lib->librara__generated_request_complete(
			req.prim_ptr(), nullptr, nullptr, nullptr, req.hash_ptr(),
			req.success());

		break;
	}
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}
