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

		friend class Trust_anchor;

		Type const _type;
		Key_value &_key_plaintext;
		Key_value &_key_ciphertext;
		Hash &_hash;
		Passphrase const _passphrase;
		bool &_success;

	public:

		Trust_anchor_request(Module_id src, Module_channel_id, Type, Key_value &, Key_value &, Hash &,
		                     Passphrase, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Trust_anchor_channel
{
	private:

		friend class Trust_anchor;

		enum State {
			INACTIVE, SUBMITTED, WRITE_PENDING, WRITE_IN_PROGRESS,
			READ_PENDING, READ_IN_PROGRESS, COMPLETE };

		Key_value _dummy_key { };
		Hash _dummy_hash { };
		bool _dummy_success { };
		State _state { INACTIVE };
		Trust_anchor_request _request { INVALID_MODULE_ID, INVALID_MODULE_CHANNEL_ID, Trust_anchor_request::INITIALIZE, _dummy_key,
		                                _dummy_key, _dummy_hash, Passphrase { }, _dummy_success };
		Vfs::file_offset _file_offset { 0 };
		size_t _file_size { 0 };
};

class Tresor::Trust_anchor : public Module
{
	private:

		using Request = Trust_anchor_request;
		using Channel = Trust_anchor_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		enum { NR_OF_CHANNELS = 1 };

		Vfs::Env          &_vfs_env;
		char               _read_buf[64];
		String<128> const  _path;
		String<128> const  _decrypt_path             { _path, "/decrypt" };
		Vfs::Vfs_handle   &_decrypt_file             { vfs_open_rw(_vfs_env, { _decrypt_path }) };
		String<128> const  _encrypt_path             { _path, "/encrypt" };
		Vfs::Vfs_handle   &_encrypt_file             { vfs_open_rw(_vfs_env, { _encrypt_path }) };
		String<128> const  _generate_key_path        { _path, "/generate_key" };
		Vfs::Vfs_handle   &_generate_key_file        { vfs_open_rw(_vfs_env, { _generate_key_path }) };
		String<128> const  _initialize_path          { _path, "/initialize" };
		Vfs::Vfs_handle   &_initialize_file          { vfs_open_rw(_vfs_env, { _initialize_path }) };
		String<128> const  _hashsum_path             { _path, "/hashsum" };
		Vfs::Vfs_handle   &_hashsum_file             { vfs_open_rw(_vfs_env, { _hashsum_path }) };
		Channel            _channels[NR_OF_CHANNELS] { };

		void
		_execute_write_read_operation(Vfs::Vfs_handle   &file,
		                              String<128> const &file_path,
		                              Channel           &channel,
		                              char const        *write_buf,
		                              char              *read_buf,
		                              size_t             read_size,
		                              bool              &progress);

		void _execute_write_operation(Vfs::Vfs_handle   &file,
		                              String<128> const &file_path,
		                              Channel           &channel,
		                              char const        *write_buf,
		                              bool              &progress,
		                              bool               result_via_read);

		void _execute_read_operation(Vfs::Vfs_handle   &file,
		                             String<128> const &file_path,
		                             Channel           &channel,
		                             char              *read_buf,
		                             bool              &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		bool new_submit_request() override { return false; }

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

		Trust_anchor(Vfs::Env       &vfs_env,
		             Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
