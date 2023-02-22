/*
 * \brief  Module for encrypting/decrypting single data blocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <trust_anchoa.h>

using namespace Genode;
using namespace Cbe;


/**************************
 ** Trust_anchoa_request **
 **************************/

void Trust_anchoa_request::create(void       *buf_ptr,
                                  size_t      buf_size,
                                  size_t      src_module_id,
                                  size_t      src_request_id,
                                  size_t      req_type,
                                  void       *prim_ptr,
                                  size_t      prim_size,
                                  void       *key_plaintext_ptr,
                                  void       *key_ciphertext_ptr,
                                  char const *passphrase_ptr,
                                  void       *hash_ptr)
{
	Trust_anchoa_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	req._passphrase_ptr = (addr_t)passphrase_ptr;
	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	if (key_plaintext_ptr != nullptr)
		memcpy(
			&req._key_plaintext, key_plaintext_ptr,
			sizeof(req._key_plaintext));

	if (key_ciphertext_ptr != nullptr)
		memcpy(
			&req._key_ciphertext, key_ciphertext_ptr,
			sizeof(req._key_ciphertext));

	if (hash_ptr != nullptr)
		memcpy(&req._hash, hash_ptr, sizeof(req._hash));

	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Trust_anchoa_request::Trust_anchoa_request(unsigned long src_module_id,
                                           unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, TRUST_ANCHOA }
{ }


char const *Trust_anchoa_request::type_name()
{
	switch (_type) {
	case INVALID: return "invalid";
	case CREATE_KEY: return "create_key";
	case ENCRYPT_KEY: return "encrypt_key";
	case DECRYPT_KEY: return "decrypt_key";
	case SECURE_SUPERBLOCK: return "secure_superblock";
	case GET_LAST_SB_HASH: return "get_last_sb_hash";
	case INITIALIZE: return "initialize";
	}
	return "?";
}


/******************
 ** Trust_anchoa **
 ******************/


void Trust_anchoa::execute(bool &)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		switch (channel._request._type) {
		default:
			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


Trust_anchoa::Trust_anchoa() { }


bool Trust_anchoa::_peek_completed_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Trust_anchoa::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


bool Trust_anchoa::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Trust_anchoa::submit_request(Module_request &req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *dynamic_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}
