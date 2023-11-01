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

#ifndef _TRESOR__CRYPTO_H_
#define _TRESOR__CRYPTO_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Crypto;
	class Crypto_request;
	class Crypto_channel;
}

class Tresor::Crypto_request : public Module_request
{
	friend class Crypto_channel;

	public:

		enum Type { ADD_KEY, REMOVE_KEY, DECRYPT, ENCRYPT, DECRYPT_CLIENT_DATA, ENCRYPT_CLIENT_DATA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Key_id const _key_id;
		Key_value const &_key_plaintext;
		Block &_plaintext_blk;
		Block &_ciphertext_blk;
		bool &_success;

		NONCOPYABLE(Crypto_request);

	public:

		Crypto_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Key_id,
		               Key_value const &, Physical_block_address, Virtual_block_address, Block &,
		               Block &, bool &);

		static const char *type_to_string(Type type);

		void print(Output &out) const override;
};

class Tresor::Crypto_channel : public Module_channel
{
	friend class Crypto;

	private:

		using Request = Crypto_request;
		using Path = String<128>;
		using Write_result = Vfs::File_io_service::Write_result;
		using Read_result = Vfs::File_io_service::Read_result;

		enum State {
			SUBMITTED, COMPLETE, OBTAIN_PLAINTEXT_BLK_PENDING,
			OBTAIN_PLAINTEXT_BLK_IN_PROGRESS, OBTAIN_PLAINTEXT_BLK_COMPLETE,
			SUPPLY_PLAINTEXT_BLK_PENDING, SUPPLY_PLAINTEXT_BLK_IN_PROGRESS,
			SUPPLY_PLAINTEXT_BLK_COMPLETE, OP_WRITTEN_TO_VFS_HANDLE,
			QUEUE_READ_SUCCEEDED, REQ_GENERATED };

		struct Key_directory
		{
			Vfs::Vfs_handle *encrypt_handle { };
			Vfs::Vfs_handle *decrypt_handle { };
			Key_id key_id { };
		};

		Vfs::Env &_vfs_env;
		Path const _path;
		Vfs::Vfs_handle &_add_key_handle { vfs_open_wo(_vfs_env, { _path, "/add_key" }) };
		Vfs::Vfs_handle &_remove_key_handle { vfs_open_wo(_vfs_env, { _path, "/remove_key" }) };
		Key_directory _key_dirs[2] { };
		State _state { COMPLETE };
		bool _generated_req_success { false };
		Vfs::Vfs_handle *_vfs_handle { nullptr };
		Block _blk { };
		Crypto_request *_req_ptr { };

		NONCOPYABLE(Crypto_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETE; }

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _add_key(bool &);

		void _remove_key(bool &);

		void _decrypt(bool &);

		void _encrypt(bool &);

		void _encrypt_client_data(bool &);

		void _decrypt_client_data(bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

		Key_directory &_lookup_key_dir(uint32_t key_id);

	public:

		Crypto_channel(Module_channel_id, Vfs::Env &, Xml_node const &);

		void execute(bool &);
};

class Tresor::Crypto : public Module
{
	private:

		using Request = Crypto_request;
		using Channel = Crypto_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Crypto);

	public:

		struct Add_key : Request
		{
			Add_key(Module_id m, Module_channel_id c, Key &k, bool &s)
			: Request(m, c, Request::ADD_KEY, 0, 0, k.id, k.value, 0, 0, *(Block*)0, *(Block*)0, s) { }
		};

		struct Remove_key : Request
		{
			Remove_key(Module_id m, Module_channel_id c, Key_id k, bool &s)
			: Request(m, c, Request::REMOVE_KEY, 0, 0, k, *(Key_value*)0, 0, 0, *(Block*)0, *(Block*)0, s) { }
		};

		struct Decrypt : Request
		{
			Decrypt(Module_id m, Module_channel_id c, Key_id k, Physical_block_address pa, Block &b, bool &s)
			: Request(m, c, Request::DECRYPT, 0, 0, k, *(Key_value*)0, pa, 0, b, b, s) { }
		};

		struct Encrypt : Request
		{
			Encrypt(Module_id m, Module_channel_id c, Key_id k, Physical_block_address pa, Block &b, bool &s)
			: Request(m, c, Request::ENCRYPT, 0, 0, k, *(Key_value*)0, pa, 0, b, b, s) { }
		};

		Crypto(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__CRYPTO_H_ */
