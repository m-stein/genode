/*
 * \brief  Module for accessing the systems trust anchor
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__TRUST_ANCHOR_H_
#define _TRESOR__TRUST_ANCHOR_H_

/* base includes */
#include <util/reconstructible.h>

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Trust_anchor;
	class Trust_anchor_request;
	class Trust_anchor_channel;
}

class Tresor::Trust_anchor_request : public Module_request
{
	public:

		enum Type { CREATE_KEY, ENCRYPT_KEY, DECRYPT_KEY, SECURE_SUPERBLOCK, GET_LAST_SB_HASH, INITIALIZE };

	private:

		friend class Trust_anchor_channel;

		Type const _type;
		Key_value &_key_plaintext;
		Key_value &_key_ciphertext;
		Hash &_hash;
		Passphrase const _passphrase;
		bool &_success;

		NONCOPYABLE(Trust_anchor_request);

	public:

		Trust_anchor_request(Module_id src, Module_channel_id, Type, Key_value &, Key_value &, Hash &,
		                     Passphrase, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Trust_anchor_channel : public Module_channel
{
	private:

		using Path = String<128>;
		using Request = Trust_anchor_request;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		enum State { REQ_SUBMITTED, WRITE_PENDING, WRITE_IN_PROGRESS, READ_PENDING, READ_IN_PROGRESS, REQ_COMPLETE };

		State _state { REQ_COMPLETE };
		Vfs::Env &_vfs_env;
		char _read_buf[64];
		Path const _path;
		Vfs::Vfs_handle &_decrypt_file { vfs_open_rw(_vfs_env, { _path, "/decrypt" }) };
		Vfs::Vfs_handle &_encrypt_file { vfs_open_rw(_vfs_env, { _path, "/encrypt" }) };
		Vfs::Vfs_handle &_generate_key_file { vfs_open_rw(_vfs_env, { _path, "/generate_key" }) };
		Vfs::Vfs_handle &_initialize_file { vfs_open_rw(_vfs_env, { _path, "/initialize" }) };
		Vfs::Vfs_handle &_hashsum_file { vfs_open_rw(_vfs_env, { _path, "/hashsum" }) };
		Trust_anchor_request *_req_ptr { nullptr };
		Vfs::file_offset _file_offset { 0 };
		size_t _file_size { 0 };

		NONCOPYABLE(Trust_anchor_channel);

		void _generated_req_completed(State_uint) override { ASSERT_NEVER_REACHED; }

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _write_read_file(Vfs::Vfs_handle &, char const *, char *, size_t, bool &);

		void _write_file(Vfs::Vfs_handle &, char const *, bool &, bool);

		void _read_file(Vfs::Vfs_handle &, char *, bool &);

	public:

		void execute(bool &);

		Trust_anchor_channel(Module_channel_id id, Vfs::Env &, Xml_node const &);
};

class Tresor::Trust_anchor : public Module
{
	private:

		using Request = Trust_anchor_request;
		using Channel = Trust_anchor_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Trust_anchor);

	public:

		struct Create_key : Request
		{
			Create_key(Module_id m, Module_channel_id c, Key_value &k, bool &s)
			: Request(m, c, Request::CREATE_KEY, k, *(Key_value*)0, *(Hash*)0, Passphrase(), s) { }
		};

		struct Encrypt_key : Request
		{
			Encrypt_key(Module_id m, Module_channel_id c, Key_value const &kp, Key_value &kc, bool &s)
			: Request(m, c, Request::ENCRYPT_KEY, *const_cast<Key_value*>(&kp), kc, *(Hash*)0, Passphrase(), s) { }
		};

		struct Decrypt_key : Request
		{
			Decrypt_key(Module_id m, Module_channel_id c, Key_value &kp, Key_value const &kc, bool &s)
			: Request(m, c, Request::DECRYPT_KEY, kp, *const_cast<Key_value*>(&kc), *(Hash*)0, Passphrase(), s) { }
		};

		struct Write_hash : Request
		{
			Write_hash(Module_id m, Module_channel_id c, Hash const &h, bool &s)
			: Request(m, c, Request::SECURE_SUPERBLOCK, *(Key_value*)0, *(Key_value*)0, *const_cast<Hash*>(&h), Passphrase(), s) { }
		};

		struct Read_hash : Request
		{
			Read_hash(Module_id m, Module_channel_id c, Hash &h, bool &s)
			: Request(m, c, Request::GET_LAST_SB_HASH, *(Key_value*)0, *(Key_value*)0, h, Passphrase(), s) { }
		};

		Trust_anchor(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
