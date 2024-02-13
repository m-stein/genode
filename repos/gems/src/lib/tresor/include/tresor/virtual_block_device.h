/*
 * \brief  Module for accessing and managing trees of the virtual block device
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VIRTUAL_BLOCK_DEVICE_H_
#define _TRESOR__VIRTUAL_BLOCK_DEVICE_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/free_tree.h>
#include <tresor/block_io.h>
#include <tresor/client_data_interface.h>
#include <tresor/crypto.h>

namespace Tresor {

	class Virtual_block_device;
	class Virtual_block_device_request;
	class Virtual_block_device_channel;
}

class Tresor::Virtual_block_device_request : public Module_request
{
	friend class Virtual_block_device_channel;

	public:

		enum Type { REKEY_VBA, READ_VBA, WRITE_VBA, EXTENSION_STEP };

	private:

		Type const _type;
		Virtual_block_address const _vba;
		Snapshots &_snapshots;
		Snapshot_index const _curr_snap_idx;
		Tree_degree const _snap_degr;
		Generation const _curr_gen;
		Key_id const _curr_key_id;
		Key_id const _prev_key_id;
		Tree_root &_ft;
		Tree_root &_mt;
		Tree_degree const _vbd_degree;
		Virtual_block_address const _vbd_highest_vba;
		bool const _rekeying;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Generation const _last_secured_gen;
		Physical_block_address &_pba;
		Number_of_blocks &_num_pbas;
		Number_of_leaves &_num_leaves;
		Virtual_block_address const _rekeying_vba;
		bool &_success;

		NONCOPYABLE(Virtual_block_device_request);

	public:

		Virtual_block_device_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Generation,
		                             Tree_root &, Tree_root &, Tree_degree, Virtual_block_address, bool,
		                             Virtual_block_address, Snapshot_index, Snapshots &, Tree_degree, Key_id,
		                             Key_id, Generation, Physical_block_address &, bool &, Number_of_leaves &,
		                             Number_of_blocks &, Virtual_block_address);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Virtual_block_device_channel : public Module_channel
{
	private:

		using Request = Virtual_block_device_request;

		enum State {
			SUBMITTED, REQ_GENERATED, REQ_COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED,
			DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		Request *_req_ptr { nullptr };
		State _state { REQ_COMPLETE };
		Snapshot_index _snap_idx { 0 };
		Type_1_node_block_walk _t1_blks { };
		Type_1_node_walk _t1_nodes { };
		Tree_level_index _lvl { 0 };
		Virtual_block_address _vba { 0 };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Hash _hash { };
		Number_of_blocks _num_blks { 0 };
		Generation _free_gen { 0 };
		Block _encoded_blk { };
		Block _data_blk { };
		bool _first_snapshot { false };
		bool _gen_req_success { false };
		union {
			Generatable_request<Virtual_block_device_channel, State, Block_io::Read> _read_block;
			Generatable_request<Virtual_block_device_channel, State, Block_io::Write> _write_block;
			Generatable_request<Virtual_block_device_channel, State, Crypto::Encrypt> _encrypt_block;
			Generatable_request<Virtual_block_device_channel, State, Crypto::Decrypt> _decrypt_block;
			Generatable_request<Virtual_block_device_channel, State, Free_tree::Allocate_pbas> _alloc_pbas;
		};

		NONCOPYABLE(Virtual_block_device_channel);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			generate_req<REQUEST>(complete_state, progress, args..., _gen_req_success);
			_state = REQ_GENERATED;
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _generated_req_completed(State_uint) override;

		void _start_alloc_pbas(bool &, Free_tree::Allocate_pbas::Application);

		Snapshot &snap() { return _req_ptr->_snapshots.items[_snap_idx]; }

		void _generate_write_blk_req(bool &);

		bool _find_next_snap_to_rekey_vba_at(Snapshot_index &) const;

		void _read_vba(Client_data_interface &, Block_io &, Crypto &, bool &);

		bool _check_and_decode_read_blk(bool &);

		Tree_node_index _node_idx(Tree_level_index, Virtual_block_address) const;

		Type_1_node &_node(Tree_level_index, Virtual_block_address);

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _set_new_pbas_and_num_blks_for_alloc();

		void _generate_ft_alloc_req_for_write_vba(bool &);

		void _write_vba(Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &, bool &);

		void _update_nodes_of_branch_of_written_vba();

		void _rekey_vba(Block_io &, Crypto &, Free_tree &, Meta_tree &, bool &);

		void _generate_ft_alloc_req_for_rekeying(Tree_level_index, bool &);

		void _add_new_root_lvl_to_snap();

		void _add_new_branch_to_snap(Tree_level_index, Tree_node_index);

		void _set_new_pbas_identical_to_curr_pbas();

		void _generate_ft_alloc_req_for_resizing(Tree_level_index, bool &);

		void _extension_step(Block_io &, Free_tree &, Meta_tree &, bool &);

	public:

		Virtual_block_device_channel(Module_channel_id id) : Module_channel(VIRTUAL_BLOCK_DEVICE, id) { }

		~Virtual_block_device_channel() { }

		void execute(Client_data_interface &, Block_io &, Crypto &, Free_tree &, Meta_tree &, bool &);

		using Module = Virtual_block_device;

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
};

class Tresor::Virtual_block_device : public Module
{
	private:

		using Channel = Virtual_block_device_channel;

		Constructible<Channel> _channels[1] { };
		Client_data_interface &_client_data;
		Block_io &_block_io;
		Crypto &_crypto;
		Free_tree &_free_tree;
		Meta_tree &_meta_tree;

		NONCOPYABLE(Virtual_block_device);

		void execute(bool &) override;

	public:

		class Rekey_vba;
		class Read_vba;
		class Write_vba;

		bool execute(Rekey_vba &, Block_io &, Crypto &, Free_tree &, Meta_tree &);
		bool execute(Read_vba &, Client_data_interface &, Block_io &, Crypto &);
		bool execute(Write_vba &, Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &);

		Virtual_block_device(Client_data_interface &, Block_io &, Crypto &, Free_tree &, Meta_tree &);

		static constexpr char const *name() { return "vbd"; }
};

class Tresor::Virtual_block_device::Rekey_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshots &in_out_snapshots;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Virtual_block_address const in_vba;
			Generation const in_curr_gen;
			Generation const in_last_secured_gen;
			Key_id const in_curr_key_id;
			Key_id const in_prev_key_id;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_highest_vba;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, WRITE_BLK, WRITE_BLK_SUCCEEDED,
			DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED,
			ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		using Helper = Request_helper<Rekey_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Block _encoded_blk { };
		Block _data_blk { };
		Generation _free_gen { 0 };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Snapshot_index _snap_idx { 0 };
		Type_1_node_walk _t1_nodes { };
		Number_of_blocks _num_blks { 0 };
		Hash _hash { };
		bool _first_snapshot { false };
		union {
			Generatable_request<Helper, State, Block_io::Read> _read_block;
			Generatable_request<Helper, State, Block_io::Write> _write_block;
			Generatable_request<Helper, State, Crypto::Encrypt> _encrypt_block;
			Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block;
			Generatable_request<Helper, State, Free_tree::Allocate_pbas> _alloc_pbas;
		};

		bool _check_and_decode_read_blk(bool &);

		void _start_alloc_pbas(bool &, Free_tree::Allocate_pbas::Application);

		void _generate_write_blk_req(bool &);

		bool _find_next_snap_to_rekey_vba_at(Snapshot_index &) const;

		void _generate_ft_alloc_req_for_rekeying(Tree_level_index, bool &);

		Snapshot &snap() { return _attr.in_out_snapshots.items[_snap_idx]; }

		Type_1_node &_node(Tree_level_index, Virtual_block_address);

		Tree_node_index _node_idx(Tree_level_index, Virtual_block_address) const;

	public:

		Rekey_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Rekey_vba() { }

		void print(Output &out) const { Genode::print(out, "rekey vba"); }

		bool execute(Block_io &, Crypto &, Free_tree &, Meta_tree &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Virtual_block_device::Read_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshot const &in_snap;
			Virtual_block_address const in_vba;
			Key_id const in_key_id;
			Tree_degree const in_vbd_degree;
			Request_offset const in_client_req_offset;
			Request_tag const in_client_req_tag;
		};

	private:

		enum State { INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED };

		using Helper = Request_helper<Read_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Hash _hash { };
		Block _blk { };
		Tree_walk_pbas _new_pbas { };
		union {
			Generatable_request<Helper, State, Block_io::Read> _read_block;
			Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block;
		};

		bool _check_and_decode_read_blk(bool &);

	public:

		Read_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Read_vba() { }

		void print(Output &out) const { Genode::print(out, "rekey vba"); }

		bool execute(Client_data_interface &, Block_io &, Crypto &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Virtual_block_device::Write_vba : Noncopyable
{
	public:

		using Module = Virtual_block_device;

		struct Attr
		{
			Snapshot &in_out_snap;
			Snapshots const &in_snapshots;
			Tree_root &in_out_ft;
			Tree_root &in_out_mt;
			Virtual_block_address const in_vba;
			Key_id const in_curr_key_id;
			Key_id const in_prev_key_id;
			Tree_degree const in_vbd_degree;
			Virtual_block_address const in_vbd_highest_vba;
			Request_offset const in_client_req_offset;
			Request_tag const in_client_req_tag;
			Generation const in_curr_gen;
			Generation const in_last_secured_gen;
			bool in_rekeying;
			Virtual_block_address const in_rekeying_vba;
		};

	private:

		enum State {
			INIT, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED, DECRYPT_BLOCK, DECRYPT_BLOCK_SUCCEEDED,
			WRITE_BLK, WRITE_BLK_SUCCEEDED, ENCRYPT_BLOCK, ENCRYPT_BLOCK_SUCCEEDED, ALLOC_PBAS, ALLOC_PBAS_SUCCEEDED };

		using Helper = Request_helper<Write_vba, State>;

		Helper _helper;
		Attr const _attr;
		Tree_level_index _lvl { 0 };
		Type_1_node_block_walk _t1_blks { };
		Hash _hash { };
		Type_1_node_walk _t1_nodes { };
		Block _data_blk { };
		Block _encoded_blk { };
		Tree_walk_pbas _new_pbas { };
		Number_of_blocks _num_blks { 0 };
		Generation _free_gen { 0 };
		union {
			Generatable_request<Helper, State, Block_io::Read> _read_block;
			Generatable_request<Helper, State, Crypto::Decrypt> _decrypt_block;
			Generatable_request<Helper, State, Crypto::Encrypt> _encrypt_block;
			Generatable_request<Helper, State, Free_tree::Allocate_pbas> _alloc_pbas;
			Generatable_request<Helper, State, Block_io::Write> _write_block;
		};

		bool _check_and_decode_read_blk(bool &);

		void _set_new_pbas_and_num_blks_for_alloc();

		void _update_nodes_of_branch_of_written_vba();

		void _generate_ft_alloc_req_for_write_vba(bool &);

		void _generate_write_blk_req(bool &);

	public:

		Write_vba(Attr const &attr) : _helper(*this), _attr(attr) { }

		~Write_vba() { }

		void print(Output &out) const { Genode::print(out, "rekey vba"); }

		bool execute(Client_data_interface &, Block_io &, Free_tree &, Meta_tree &, Crypto &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

#endif /* _TRESOR__VIRTUAL_BLOCK_DEVICE_H_ */
