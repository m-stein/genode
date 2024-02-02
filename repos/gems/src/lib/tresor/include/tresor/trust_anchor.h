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
#include <tresor/file.h>

namespace Tresor {

	class Trust_anchor;
	class Trust_anchor_request;
	class Trust_anchor_channel;
}

class Tresor::Trust_anchor_request : public Module_request
{
	friend class Trust_anchor_channel;

	public:

		enum Type { CREATE_KEY, ENCRYPT_KEY, DECRYPT_KEY, WRITE_HASH, READ_HASH, INITIALIZE };

	private:

		Type const _type;
		Key_value &_key_plaintext;
		Key_value &_key_ciphertext;
		Hash &_hash;
		Passphrase const _pass;
		bool &_success;

		NONCOPYABLE(Trust_anchor_request);

	public:

		Trust_anchor_request(Module_id src, Module_channel_id, Type, Key_value &, Key_value &, Hash &, Passphrase, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Trust_anchor_channel : public Module_channel
{
	private:

		using Request = Trust_anchor_request;

		enum State { REQ_SUBMITTED, REQ_COMPLETE, READ_OK, WRITE_OK, FILE_ERR  };

		State _state { REQ_COMPLETE };
		Vfs::Env &_vfs_env;
		char _result_buf[3];
		Tresor::Path const _path;
		Read_write_file<State> _decrypt_file { _state, _vfs_env, { _path, "/decrypt" } };
		Read_write_file<State> _encrypt_file { _state, _vfs_env, { _path, "/encrypt" } };
		Read_write_file<State> _generate_key_file { _state, _vfs_env, { _path, "/generate_key" } };
		Read_write_file<State> _initialize_file { _state, _vfs_env, { _path, "/initialize" } };
		Read_write_file<State> _hash_file { _state, _vfs_env, { _path, "/hash" } };
		Trust_anchor_request *_req_ptr { nullptr };

		NONCOPYABLE(Trust_anchor_channel);

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _create_key(bool &);

		void _read_hash(bool &);

		void _initialize(bool &);

		void _write_hash(bool &);

		void _encrypt_key(bool &);

		void _decrypt_key(bool &);

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

	public:

		void execute(bool &);

		Trust_anchor_channel(Module_channel_id, Vfs::Env &, Xml_node const &);
};

class Tresor::Trust_anchor : public Module
{
	public:

		struct Attr
		{
			Vfs::Vfs_handle &decrypt_file;
			Vfs::Vfs_handle &encrypt_file;
			Vfs::Vfs_handle &generate_key_file;
			Vfs::Vfs_handle &initialize_file;
			Vfs::Vfs_handle &hash_file;
		};

	private:

		using Request = Trust_anchor_request;
		using Channel = Trust_anchor_channel;

		Constructible<Channel> _channels[1] { };
		Attr const _attr;
		addr_t _user { };

		NONCOPYABLE(Trust_anchor);

	public:

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

		class Generate_key;
		class Initialize;
		class Read_hash;
		class Write_hash;

		Trust_anchor(Vfs::Env &, Xml_node const &, Attr const &);

		void execute(bool &) override;

		template <typename REQ>
		bool execute(REQ &req)
		{
			if (!_user)
				_user = (addr_t)&req;

			if (_user != (addr_t)&req)
				return false;

			bool progress = req.execute(_attr);
			if (req.complete())
				_user = 0;

			return progress;
		}

		static constexpr char const *name() { return "trust_anchor"; }
};

class Tresor::Trust_anchor::Initialize
{
	public:

		using Module = Trust_anchor;

		struct Attr { Passphrase const &in_passphrase; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Initialize, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };
		char _result_buf[3];

		NONCOPYABLE(Initialize);

	public:

		Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "initialize"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Generate_key
{
	public:

		using Module = Trust_anchor;

		struct Attr { Key_value &out_key_plaintext; };

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Generate_key, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

		NONCOPYABLE(Generate_key);

	public:

		Generate_key(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "create key"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Write_hash
{
	public:

		using Module = Trust_anchor;

		struct Attr { Hash const &in_hash; };

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, READ_OK, FILE_ERR };

		Request_helper<Write_hash, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };
		char _result_buf[3];

		NONCOPYABLE(Write_hash);

	public:

		Write_hash(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "write hash"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Trust_anchor::Read_hash
{
	public:

		using Module = Trust_anchor;

		struct Attr { Hash &out_hash; };

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Read_hash, State> _helper;
		Attr const _attr;
		Constructible<File<State> > _file { };

		NONCOPYABLE(Read_hash);

	public:

		Read_hash(Attr const &attr) : _helper(*this), _attr(attr) { }

		void print(Output &out) const { Genode::print(out, "read hash"); }

		bool execute(Trust_anchor::Attr const &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__TRUST_ANCHOR_H_ */
