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

#ifndef _TRUST_ANCHOA_H_
#define _TRUST_ANCHOA_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Trust_anchoa;
	class Trust_anchoa_request;
	class Trust_anchoa_channel;
}

class Cbe::Trust_anchoa_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, CREATE_KEY = 1, ENCRYPT_KEY = 2, DECRYPT_KEY = 3,
			SECURE_SUPERBLOCK = 4, GET_LAST_SB_HASH = 5, INITIALIZE = 6 };

	private:

		friend class Trust_anchoa;
		friend class Trust_anchoa_channel;

		Type            _type                     { INVALID };
		Genode::uint8_t _prim[PRIM_BUF_SIZE]      { 0 };
		Genode::uint8_t _key_plaintext[KEY_SIZE]  { 0 };
		Genode::uint8_t _key_ciphertext[KEY_SIZE] { 0 };
		Genode::uint8_t _hash[HASH_SIZE]          { 0 };
		Genode::addr_t  _passphrase_ptr           { nullptr };
		uint32_t        _tag                      { 0 };
		bool            _success                  { false };

	public:

		Trust_anchoa_request() { }

		Trust_anchoa_request(unsigned long src_module_id,
		                     unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   size_t            prim_size,
		                   void             *key_plaintext_ptr,
		                   void             *key_ciphertext_ptr,
		                   char const       *passphrase_ptr,
		                   Genode::uint32_t  tag,
		                   void             *hash_ptr);

		void *prim() { return (void *)&_prim; }


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override;
};

class Cbe::Trust_anchoa_channel
{
	private:

		friend class Trust_anchoa;

		enum State { INACTIVE, SUBMITTED, COMPLETE };

		State                _state   { INACTIVE };
		Trust_anchoa_request _request { };
};

class Cbe::Trust_anchoa : public Module
{
	private:

		using Request = Trust_anchoa_request;
		using Channel = Trust_anchoa_channel;

		enum { NR_OF_CHANNELS = 4 };

		Channel _channels[NR_OF_CHANNELS] { };


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

	public:

		Trust_anchoa();


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;
};

#endif /* _TRUST_ANCHOA_H_ */
