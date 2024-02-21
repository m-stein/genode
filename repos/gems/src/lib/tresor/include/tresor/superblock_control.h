/*
 * \brief  Module for accessing and managing the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SUPERBLOCK_CONTROL_H_
#define _TRESOR__SUPERBLOCK_CONTROL_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/virtual_block_device.h>
#include <tresor/trust_anchor.h>
#include <tresor/block_io.h>

namespace Tresor {

	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Tresor::Superblock_control_request : Module_request, Noncopyable
{
	friend class Superblock_control_channel;

	public:

		enum Type {
			READ_VBA, WRITE_VBA, SYNC, INITIALIZE, DEINITIALIZE, VBD_EXTENSION_STEP,
			FT_EXTENSION_STEP, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, INITIALIZE_REKEYING,
			REKEY_VBA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Number_of_blocks _nr_of_blks;
		Virtual_block_address const _vba;
		bool &_success;
		bool &_client_req_finished;
		Superblock::State &_sb_state;
		Generation &_gen;

	public:

		Superblock_control_request(Module_id, Module_channel_id, Type, Request_offset,
		                           Request_tag, Number_of_blocks, Virtual_block_address,
		                           bool &, bool &, Superblock::State &, Generation &);

		static char const *type_to_string(Type);

		void print(Output &) const override;
};


class Tresor::Superblock_control_channel : public Module_channel
{
	private:

		using Request = Superblock_control_request;

		enum State : State_uint {
			INACTIVE, REQ_SUBMITTED, READ_VBA, READ_VBA_SUCCEEDED, WRITE_VBA, WRITE_VBA_SUCCEEDED,
			REKEY_VBA, REKEY_VBA_SUCCEEDED, GENERATE_KEY, GENERATE_KEY_SUCCEEDED,
			EXTEND_VBD, EXTEND_FREE_TREE, EXTEND_TREE_SUCCEEDED, DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED,
			DECRYPT_PREV_KEY_SUCCEEDED, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, ADD_KEY, ADD_PREV_KEY_SUCCEEDED,
			ADD_CURR_KEY_SUCCEEDED, REMOVE_KEY, REMOVE_PREV_KEY_SUCCEEDED, REMOVE_CURR_KEY_SUCCEEDED,
			READ_BLOCK, READ_SB_SUCCEEDED, REQ_COMPLETE, REQ_GENERATED, SECURE_SB, SECURE_SB_SUCCEEDED };

		enum Secure_sb_state : State_uint {
			SECURE_SB_INACTIVE, STARTED, ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED,
			SECURE_SB_REQ_GENERATED, ENCRYPT_PREV_KEY_SUCCEEDED, SYNC_CACHE_SUCCEEDED,
			WRITE_BLOCK, WRITE_SB_SUCCEEDED, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED, WRITE_SB_HASH,
			WRITE_SB_HASH_SUCCEEDED };

		State _state { INACTIVE };
		Constructible<Tree_root> _ft { };
		Constructible<Tree_root> _mt { };
		Secure_sb_state _secure_sb_state { SECURE_SB_INACTIVE };
		Superblock _sb_ciphertext { };
		Block _blk { };
		Generation _gen { };
		Hash _hash { };
		Physical_block_address _pba { INVALID_PBA };
		Number_of_blocks _nr_of_leaves { 0 };
		Request *_req_ptr { nullptr };
		bool _gen_req_success { false };
		Superblock &_sb;
		Superblock_index &_sb_idx;
		Generation &_curr_gen;
		union {
			Generatable_request<Superblock_control_channel, State, Block_io::Read> _read_block;
			Generatable_request<Superblock_control_channel, Secure_sb_state, Block_io::Write> _write_block;
			Generatable_request<Superblock_control_channel, Secure_sb_state, Block_io::Sync> _sync_block_io;
			Generatable_request<Superblock_control_channel, State, Crypto::Remove_key> _remove_key;
			Generatable_request<Superblock_control_channel, State, Crypto::Add_key> _add_key;
			Generatable_request<Superblock_control_channel, State, Trust_anchor::Generate_key> _generate_key;
			Generatable_request<Superblock_control_channel, State, Trust_anchor::Read_hash> _read_sb_hash;
			Generatable_request<Superblock_control_channel, State, Trust_anchor::Decrypt_key> _decrypt_key;
			Generatable_request<Superblock_control_channel, Secure_sb_state, Trust_anchor::Encrypt_key> _encrypt_key;
			Generatable_request<Superblock_control_channel, Secure_sb_state, Trust_anchor::Write_hash> _write_sb_hash;
			Generatable_request<Superblock_control_channel, State, Free_tree::Extend_tree> _extend_free_tree;
			Generatable_request<Superblock_control_channel, State, Virtual_block_device::Rekey_vba> _rekey_vba;
			Generatable_request<Superblock_control_channel, State, Virtual_block_device::Read_vba> _read_vba;
			Generatable_request<Superblock_control_channel, State, Virtual_block_device::Write_vba> _write_vba;
			Generatable_request<Superblock_control_channel, State, Virtual_block_device::Extend_tree> _extend_vbd;
		};

		NONCOPYABLE(Superblock_control_channel);

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _do_write_vba(Virtual_block_device &, Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &, bool &);

		void _do_read_vba(Virtual_block_device &, Client_data_interface &, Block_io &, Crypto &, bool &);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			generate_req<REQUEST>(complete_state, progress, args..., _gen_req_success);
			if (_state == SECURE_SB)
				_secure_sb_state = SECURE_SB_REQ_GENERATED;
			else
				_state = REQ_GENERATED;
		}

		void _start_secure_sb(bool &);

		void _secure_sb(Block_io &, Trust_anchor &, bool &);

		void _tree_ext_step(Block_io &, Trust_anchor &, Free_tree &, Meta_tree &, Virtual_block_device &, Superblock::State, bool, bool &);

		void _do_rekey_vba(Block_io &, Crypto &, Trust_anchor &, Free_tree &, Meta_tree &, Virtual_block_device &, bool &);

		void _init_rekeying(Block_io &, Crypto &, Trust_anchor &, bool &);

		void _discard_snap(Block_io &, Trust_anchor &, bool &);

		void _create_snap(Block_io &, Trust_anchor &, bool &);

		void _sync(Block_io &, Trust_anchor &, bool &);

		void _initialize(Block_io &, Crypto &, Trust_anchor &, bool &);

		void _deinitialize(Block_io &, Crypto &, Trust_anchor &, bool &);

	public:

		void execute(Block_io &, Crypto &, Trust_anchor &, Free_tree &, Meta_tree &, Virtual_block_device &,
		             Client_data_interface &, bool &);

		Superblock_control_channel(Module_channel_id, Superblock &, Superblock_index &, Generation &);

		~Superblock_control_channel() { }

		using Module = Superblock_control;

		void generated_req_failed(bool &progress) { _mark_req_failed(progress, "generated request failed"); }

		void generated_req_succeeded(State target_state, bool &progress)
		{
			_state = target_state;
			progress = true;
		}

		void req_generated(State target_state, bool &progress)
		{
			_state = target_state;
			progress = true;
		}

		void generated_req_succeeded(Secure_sb_state target_state, bool &progress)
		{
			_secure_sb_state = target_state;
			progress = true;
		}

		void req_generated(Secure_sb_state target_state, bool &progress)
		{
			_secure_sb_state = target_state;
			progress = true;
		}
};

class Tresor::Superblock_control : public Module
{
	private:

		using Channel = Superblock_control_channel;

		class Secure_superblock : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Superblock_index &sb_idx;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
				};

			private:

				enum State {
					INIT, COMPLETE, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED,
					ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED, ENCRYPT_PREV_KEY_SUCCEEDED, WRITE_SB_HASH,
					WRITE_SB_HASH_SUCCEEDED };

				using Helper = Request_helper<Secure_superblock, State>;

				Helper _helper;
				Attr const _attr;
				Superblock _sb_ciphertext { };
				Block _blk { };
				Hash _hash { };
				Generation _gen { };
				union {
					Generatable_request<Helper, State, Block_io::Write> _write_block;
					Generatable_request<Helper, State, Block_io::Sync> _sync_block_io;
					Generatable_request<Helper, State, Trust_anchor::Encrypt_key> _encrypt_key;
					Generatable_request<Helper, State, Trust_anchor::Write_hash> _write_sb_hash;
				};

			public:

				Secure_superblock(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Secure_superblock() { }

				void print(Output &out) const { Genode::print(out, "secure sb"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		Superblock _sb { };
		Superblock_index _sb_idx { INVALID_SB_IDX };
		Generation _curr_gen { };
		Constructible<Channel> _channels[1] { };
		Block_io &_block_io;
		Crypto &_crypto;
		Trust_anchor &_trust_anchor;
		Free_tree &_free_tree;
		Meta_tree &_meta_tree;
		Virtual_block_device &_vbd;
		Client_data_interface &_client_data;

		void execute(bool &) override;

	public:

		class Write_vba : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Virtual_block_address const in_vba;
					Request_offset const in_client_req_offset;
					Request_tag const in_client_req_tag;
				};

				struct Execute_attr
				{
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Crypto &crypto;
					Superblock &sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, WRITE_VBA, WRITE_VBA_SUCCEEDED };

				using Helper = Request_helper<Write_vba, State>;

				Helper _helper;
				Attr const _attr;
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				Generatable_request<Helper, State, Virtual_block_device::Write_vba> _write_vba { };

			public:

				Write_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Write_vba() { }

				void print(Output &out) const { Genode::print(out, "write vba"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Continue_rekeying : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { bool &out_rekeying_finished; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Virtual_block_device &vbd;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, REKEY_VBA, REKEY_VBA_SUCCEEDED, SECURE_SB, SECURE_SB_SUCCEEDED,
					REMOVE_KEY, REMOVE_KEY_SUCCEEDED };

				using Helper = Request_helper<Continue_rekeying, State>;

				Helper _helper;
				Attr const _attr;
				Constructible<Tree_root> _ft { };
				Constructible<Tree_root> _mt { };
				union {
					Generatable_request<Helper, State, Virtual_block_device::Rekey_vba> _rekey_vba;
					Generatable_request<Helper, State, Crypto::Remove_key> _remove_key;
					Generatable_request<Helper, State, Secure_superblock> _secure_sb;
				};

			public:

				Continue_rekeying(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Continue_rekeying() { }

				void print(Output &out) const { Genode::print(out, "rekey vba"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Read_vba : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr
				{
					Virtual_block_address const in_vba;
					Request_offset const in_client_req_offset;
					Request_tag const in_client_req_tag;
				};

				struct Execute_attr
				{
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Crypto &crypto;
					Superblock const sb;
					Generation const &curr_gen;
				};

			private:

				enum State { INIT, COMPLETE, READ_VBA, READ_VBA_SUCCEEDED };

				using Helper = Request_helper<Read_vba, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Virtual_block_device::Read_vba> _read_vba { };

			public:

				Read_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Read_vba() { }

				void print(Output &out) const { Genode::print(out, "read vba"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Start_rekeying : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED, GENERATE_KEY, GENERATE_KEY_SUCCEEDED,
					ADD_KEY, ADD_KEY_SUCCEEDED };

				using Helper = Request_helper<Start_rekeying, State>;

				Helper _helper;
				Attr const _attr;
				Generation _gen { };
				union {
					Generatable_request<Helper, State, Secure_superblock> _secure_sb;
					Generatable_request<Helper, State, Trust_anchor::Generate_key> _generate_key;
					Generatable_request<Helper, State, Crypto::Add_key> _add_key;
				};

			public:

				Start_rekeying(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Start_rekeying() { }

				void print(Output &out) const { Genode::print(out, "start rekeying"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Create_snapshot : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Generation &out_gen; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Create_snapshot, State>;

				Helper _helper;
				Attr const _attr;
				Generation _gen { };
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Create_snapshot(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Create_snapshot() { }

				void print(Output &out) const { Genode::print(out, "create snapshot"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Synchronize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Synchronize, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Synchronize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Synchronize() { }

				void print(Output &out) const { Genode::print(out, "sync"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Deinitialize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { };

				struct Execute_attr
				{
					Superblock &sb;
					Generation const &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED, REMOVE_KEY,
					REMOVE_CURR_KEY_SUCCEEDED, REMOVE_PREV_KEY_SUCCEEDED };

				using Helper = Request_helper<Deinitialize, State>;

				Helper _helper;
				Attr const _attr;
				union {
					Generatable_request<Helper, State, Secure_superblock> _secure_sb;
					Generatable_request<Helper, State, Crypto::Remove_key> _remove_key;
				};

			public:

				Deinitialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Deinitialize() { }

				void print(Output &out) const { Genode::print(out, "deinitialize"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Initialize : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Superblock::State &out_sb_state; };

				struct Execute_attr
				{
					Superblock &sb;
					Superblock_index &sb_idx;
					Generation &curr_gen;
					Block_io &block_io;
					Crypto &crypto;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, READ_BLOCK, READ_BLOCK_SUCCEEDED,
					DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED, DECRYPT_PREV_KEY_SUCCEEDED, ADD_KEY,
					ADD_CURR_KEY_SUCCEEDED, ADD_PREV_KEY_SUCCEEDED };

				using Helper = Request_helper<Initialize, State>;

				Helper _helper;
				Attr const _attr;
				Generation _gen { };
				Hash _hash { };
				Block _blk { };
				Superblock _sb_ciphertext { };
				union {
					Generatable_request<Helper, State, Block_io::Read> _read_block;
					Generatable_request<Helper, State, Trust_anchor::Read_hash> _read_sb_hash;
					Generatable_request<Helper, State, Trust_anchor::Decrypt_key> _decrypt_key;
					Generatable_request<Helper, State, Crypto::Add_key> _add_key;
				};

			public:

				Initialize(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Initialize() { }

				void print(Output &out) const { Genode::print(out, "initialize"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Discard_snapshot : Noncopyable
		{
			public:

				using Module = Superblock_control;

				struct Attr { Generation const in_gen; };

				struct Execute_attr
				{
					Superblock &sb;
					Generation &curr_gen;
					Block_io &block_io;
					Trust_anchor &trust_anchor;
					Superblock_control &sb_control;
				};

			private:

				enum State { INIT, COMPLETE, SECURE_SB, SECURE_SB_SUCCEEDED };

				using Helper = Request_helper<Discard_snapshot, State>;

				Helper _helper;
				Attr const _attr;
				Generatable_request<Helper, State, Secure_superblock> _secure_sb { };

			public:

				Discard_snapshot(Attr const &attr) : _helper(*this), _attr(attr) { }

				~Discard_snapshot() { }

				void print(Output &out) const { Genode::print(out, "discard snapshot"); }

				bool execute(Execute_attr const &);

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		Virtual_block_address max_vba() const { return _sb.valid() ? _sb.max_vba() : 0; };

		Virtual_block_address resizing_nr_of_pbas() const { return _sb.resizing_nr_of_pbas; }

		Virtual_block_address rekeying_vba() const { return _sb.rekeying_vba; }

		Snapshots_info snapshots_info() const;

		Superblock_info sb_info() const;

		Superblock_control(Block_io &, Crypto &, Trust_anchor &, Free_tree &, Meta_tree &, Virtual_block_device &, Client_data_interface &);

		bool execute(Start_rekeying &req, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, crypto, trust_anchor, *this });
		}

		bool execute(Continue_rekeying &req, Virtual_block_device &vbd, Free_tree &free_tree, Meta_tree &meta_tree, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({ _sb, _curr_gen, block_io, crypto, trust_anchor, free_tree, meta_tree, vbd, *this });
		}

		bool execute(Secure_superblock &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _sb_idx, _curr_gen, block_io, trust_anchor });
		}

		bool execute(Synchronize &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Deinitialize &req, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, crypto, trust_anchor, *this });
		}

		bool execute(Initialize &req, Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _sb_idx, _curr_gen, block_io, crypto, trust_anchor, *this });
		}

		bool execute(Create_snapshot &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Discard_snapshot &req, Block_io &block_io, Trust_anchor &trust_anchor)
		{
			return req.execute({_sb, _curr_gen, block_io, trust_anchor, *this });
		}

		bool execute(Read_vba &req, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Crypto &crypto)
		{
			return req.execute({ vbd, client_data, block_io, crypto, _sb, _curr_gen });
		}

		bool execute(Write_vba &req, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto)
		{
			return req.execute({ vbd, client_data, block_io, free_tree, meta_tree, crypto, _sb, _curr_gen });
		}

		static constexpr char const *name() { return "sb_control"; }
};

#endif /* _TRESOR__SUPERBLOCK_CONTROL_H_ */
