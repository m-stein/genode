/*
 * \brief  Integration of the Tresor block encryption
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/dir_file_system.h>
#include <vfs/single_file_system.h>
#include <util/arg_string.h>
#include <util/xml_generator.h>
#include <trace/timestamp.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/client_data_interface.h>
#include <tresor/crypto.h>
#include <tresor/free_tree.h>
#include <tresor/meta_tree.h>
#include <tresor/superblock_control.h>
#include <tresor/trust_anchor.h>
#include <tresor/virtual_block_device.h>

/* vfs tresor includes */
#include <splitter.h>

using namespace Genode;
using namespace Vfs;
using namespace Tresor;

namespace Vfs_tresor {

	enum { VERBOSE = 1 };

	template <typename> class Schedule;
	class Request_interface;
	class Data_operation;
	class Data_operation_system;
	class Extend_operation;
	class Extend_file_system;
	class Extend_progress_file_system;
	class Rekeying;
	class Rekey_file_system;
	class Rekey_progress_file_system;
	class Deinitialize;
	class Deinitialize_file_system;
	class Control_local_factory;
	class Control_file_system;
	class Current_local_factory;
	class Current_file_system;
	class Local_factory;
	class File_system;
	class Tresor_adapter;
	class Crypto_key;
	class Initialized_tresor_adapter_interface;
}

template <typename T>
class Vfs_tresor::Schedule : Noncopyable
{
	public:

		using Item = List_element<T>;

	private:

		Item *_tail_ptr { };
		List<Item> _list { };

	public:


		void add_tail(Item &item)
		{
			_list.insert(&item, _tail_ptr);
			_tail_ptr = &item;
		}

		bool empty() const { return !_list.first(); }

		template <typename FN>
		void with_head(FN && fn)
		{
			if (_list.first())
				fn(*_list.first()->object());
		}

		void remove_head()
		{
			Item *head_ptr = _list.first();
			if (!head_ptr)
				return;

			_list.remove(head_ptr);
			if (_tail_ptr == head_ptr)
				_tail_ptr = _list.first();
		}

		template <typename CAN_YIELD_TO_FN>
		void try_yield_head(CAN_YIELD_TO_FN && can_yield_to)
		{
			Item *head_ptr = _list.first();
			if (!head_ptr)
				return;

			Item *next_ptr = head_ptr->List<Item>::Element::next();
			if (!next_ptr || !can_yield_to(*next_ptr->object()))
				return;

			remove_head();
			_list.insert(head_ptr, next_ptr);
		}
};

struct Vfs_tresor::Request_interface
{
	struct Execute_attr
	{
		Initialized_tresor_adapter_interface &adapter;
		Splitter &splitter;
		Superblock_control &sb_control;
		Client_data_interface &client_data;
		Virtual_block_device &vbd;
		Free_tree &free_tree;
		Meta_tree &meta_tree;
		Block_io &block_io;
		Crypto &crypto;
		Trust_anchor &trust_anchor;
	};

	enum Scheduling_state { REMOVE_FROM_SCHEDULE, CAN_YIELD, CANNOT_YIELD };

	virtual bool execute(Execute_attr const &attr) = 0;

	virtual Scheduling_state scheduling_state() const = 0;

	virtual bool can_be_yielded_to() const = 0;

	virtual ~Request_interface() { };
};

struct Vfs_tresor::Initialized_tresor_adapter_interface
{
	virtual bool exceeds_data_file_range(addr_t, size_t) const = 0;

	virtual size_t data_file_size() const = 0;

	virtual void add_to_schedule(Schedule<Request_interface>::Item &) = 0;

	virtual bool execute() = 0;

	virtual void extend_fs_trigger_watch_response() = 0;

	virtual void extend_progress_fs_trigger_watch_response() = 0;

	virtual void rekey_fs_trigger_watch_response() = 0;

	virtual void rekey_progress_fs_trigger_watch_response() = 0;

	virtual void deinit_fs_trigger_watch_response() = 0;

	virtual ~Initialized_tresor_adapter_interface() { };
};

class Vfs_tresor::Data_operation : Noncopyable
{
	public:

		enum Result { PENDING, SUCCEEDED, FAILED };

		struct Execute_attr
		{
			Splitter &splitter;
			Superblock_control &sb_control;
			Client_data_interface &client_data;
			Virtual_block_device &vbd;
			Free_tree &free_tree;
			Meta_tree &meta_tree;
			Block_io &block_io;
			Crypto &crypto;
			Trust_anchor &trust_anchor;
		};

	private:

		enum State {
			INIT, READ_REQUESTED, READ, READ_COMPLETE, WRITE_REQUESTED, WRITE, WRITE_COMPLETE,
			SYNC_REQUESTED, SYNC, SYNC_COMPLETE };

		State _state { INIT };
		Generation _generation { };
		addr_t _seek { };
		bool _success { };
		Constructible<Byte_range_ptr> _dst { };
		Constructible<Const_byte_range_ptr> _src { };
		Constructible<Splitter::Write> _write { };
		Constructible<Splitter::Read> _read { };
		Constructible<Superblock_control::Synchronize> _sync { };

		bool _range_violation(Superblock_control &sb_control, addr_t start, size_t num_bytes) const
		{
			addr_t last_byte = num_bytes ? start - 1 + num_bytes : start;
			addr_t last_file_byte = (sb_control.max_vba() * BLOCK_SIZE) + BLOCK_SIZE - 1;
			return last_byte > last_file_byte;
		}

	public:

		Result file_write(addr_t seek, Const_byte_range_ptr const &src)
		{
			switch (_state) {
			case INIT:

				_seek = seek;
				_src.construct(src.start, src.num_bytes);
				_state = WRITE_REQUESTED;
				return PENDING;

			case WRITE_REQUESTED:
			case WRITE: return PENDING;
			case WRITE_COMPLETE:

				_src.destruct();
				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		Result file_read(addr_t seek, Byte_range_ptr const &dst)
		{
			switch (_state) {
			case INIT:

				_seek = seek;
				_dst.construct(dst.start, dst.num_bytes);
				_state = READ_REQUESTED;
				return PENDING;

			case READ_REQUESTED:
			case READ: return PENDING;
			case READ_COMPLETE:

				_dst.destruct();
				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		Result file_sync()
		{
			switch (_state) {
			case INIT:

				_state = SYNC_REQUESTED;
				return PENDING;

			case SYNC_REQUESTED:
			case SYNC: return PENDING;
			case SYNC_COMPLETE:

				_state = INIT;
				return _success ? SUCCEEDED : FAILED;

			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		bool pending() const { return _state != INIT; }

		bool execute(Execute_attr const &attr)
		{
			bool progress = false;
			switch (_state) {
			case WRITE_REQUESTED:

				if (_range_violation(attr.sb_control, _seek, _src->num_bytes)) {
					_success = false;
					_state = WRITE_COMPLETE;
					progress = true;
					if (VERBOSE)
						log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") failed: range violation");
					break;
				}
				_write.construct(Splitter::Write::Attr{_seek, _generation, _src->start, _src->num_bytes});
				_state = WRITE;
				progress = true;
				if (VERBOSE)
					log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") started");
				break;

			case WRITE:

				progress |= attr.splitter.execute(
					*_write, {attr.sb_control, attr.vbd, attr.client_data, attr.block_io,
					          attr.free_tree, attr.meta_tree, attr.crypto});

				if (_write->complete()) {
					_success = _write->success();
					_write.destruct();
					_state = WRITE_COMPLETE;
					progress = true;
					if (VERBOSE)
						log("write (seek ", _seek, " num_bytes ", _src->num_bytes, ") ", _success ? "succeeded" : "failed");
				}
				break;

			case READ_REQUESTED:

				if (_range_violation(attr.sb_control, _seek, _dst->num_bytes)) {
					_success = false;
					_state = READ_COMPLETE;
					progress = true;
					if (VERBOSE)
						log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") failed: range violation");
					break;
				}
				_read.construct(Splitter::Read::Attr{_seek, _generation, _dst->start, _dst->num_bytes});
				_state = READ;
				progress = true;
				if (VERBOSE)
					log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") started");
				break;

			case READ:

				progress |= attr.splitter.execute(
					*_read, {attr.sb_control, attr.vbd, attr.client_data, attr.block_io, attr.crypto});

				if (_read->complete()) {
					_success = _read->success();
					_read.destruct();
					_state = READ_COMPLETE;
					progress = true;
					if (VERBOSE)
						log("read (seek ", _seek, " num_bytes ", _dst->num_bytes, ") ", _success ? "succeeded" : "failed");
				}
				break;

			case SYNC_REQUESTED:

				_sync.construct(Superblock_control::Synchronize::Attr{});
				_state = SYNC;
				progress = true;
				if (VERBOSE)
					log("sync started");
				break;

			case SYNC:

				progress |= attr.sb_control.execute(*_sync, attr.block_io, attr.trust_anchor);
				if (_sync->complete()) {
					_success = _sync->success();
					_sync.destruct();
					_state = SYNC_COMPLETE;
					progress = true;
					if (VERBOSE)
						log("sync ", _success ? "succeeded" : "failed");
				}
				break;

			default: break;
			}
			return progress;
		}
};

class Vfs_tresor::Rekeying : Noncopyable, Request_interface
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

	private:

		enum State { INIT, REKEY, REKEY_SUCCEEDED, COMPLETE };

		State _state { INIT };
		Result _last_result { NONE };
		bool _rekeying_finished { };
		Constructible<Superblock_control::Rekey> _rekey { };
		Schedule<Request_interface>::Item _schedule_item { this };

		/***********************
		 ** Request_interface **
		 ***********************/

		bool execute(Execute_attr const &attr) override
		{
			bool progress = false;
			switch (_state) {
			case REKEY:

				progress = attr.sb_control.execute(
					*_rekey, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.crypto, attr.trust_anchor);

				if (_rekey->complete()) {
					if (_rekey->success()) {
						if (_rekeying_finished) {
							_last_result = SUCCEEDED;
							_state = COMPLETE;
							if (VERBOSE)
								log("rekeying succeeded");
						} else
							_state = REKEY_SUCCEEDED;
					} else {
						_last_result = FAILED;
						_state = COMPLETE;
						if (VERBOSE)
							log("rekeying failed");
					}
					_rekey.destruct();
					attr.adapter.rekey_fs_trigger_watch_response();
					progress = true;
				}
				break;

			case REKEY_SUCCEEDED:

				_rekey.construct(Superblock_control::Rekey::Attr{_rekeying_finished});
				_state = REKEY;
				progress = true;
				break;

			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		Scheduling_state scheduling_state() const override
		{
			switch (_state) {
			case REKEY: return CANNOT_YIELD;
			case REKEY_SUCCEEDED: return CAN_YIELD;
			case COMPLETE: return REMOVE_FROM_SCHEDULE;
			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		bool can_be_yielded_to() const override { return false; };

	public:

		bool try_start(Initialized_tresor_adapter_interface &adapter)
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_last_result = PENDING;
				_rekeying_finished = false;
				_rekey.construct(Superblock_control::Rekey::Attr{_rekeying_finished});
				_state = REKEY;
				adapter.add_to_schedule(_schedule_item);
				if (VERBOSE)
					log("rekeying started");

				return true;

			default: break;
			}
			return false;
		}

		Result last_result() { return _last_result; }
};

class Vfs_tresor::Deinitialize : Noncopyable, Request_interface
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

	private:

		enum State { INIT, DEINIT_SB_CONTROL, DEINIT_SB_CONTROL_SUCCEEDED, DEINIT_SB_CONTROL_FAILED };

		State _state { INIT };
		Constructible<Superblock_control::Deinitialize> _deinit_sb_control { };
		Schedule<Request_interface>::Item _schedule_item { this };

		/***********************
		 ** Request_interface **
		 ***********************/

		bool execute(Execute_attr const &attr) override
		{
			bool progress = false;
			switch (_state) {
			case DEINIT_SB_CONTROL:

				progress = attr.sb_control.execute(
					*_deinit_sb_control, attr.block_io, attr.crypto, attr.trust_anchor);

				if (_deinit_sb_control->complete()) {
					if (_deinit_sb_control->success()) {
						_state = DEINIT_SB_CONTROL_SUCCEEDED;
						if (VERBOSE)
							log("deinitialize succeeded");
					} else {
						_state = DEINIT_SB_CONTROL_FAILED;
						if (VERBOSE)
							log("deinitialize failed");
					}
					_deinit_sb_control.destruct();
					attr.adapter.rekey_fs_trigger_watch_response();
					progress = true;
				}
				break;

			case DEINIT_SB_CONTROL_SUCCEEDED:

				_deinit_sb_control.construct(Superblock_control::Deinitialize::Attr{});
				_state = DEINIT_SB_CONTROL;
				progress = true;
				break;

			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		Scheduling_state scheduling_state() const override
		{
			switch (_state) {
			case DEINIT_SB_CONTROL: return CANNOT_YIELD;
			case DEINIT_SB_CONTROL_SUCCEEDED:
			case DEINIT_SB_CONTROL_FAILED: return REMOVE_FROM_SCHEDULE;
			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		bool can_be_yielded_to() const override { return false; };

	public:

		bool try_start(Initialized_tresor_adapter_interface &adapter)
		{
			switch (_state) {
			case INIT:
			case DEINIT_SB_CONTROL_FAILED:
			case DEINIT_SB_CONTROL_SUCCEEDED:

				_deinit_sb_control.construct(Superblock_control::Deinitialize::Attr{});
				_state = DEINIT_SB_CONTROL;
				adapter.add_to_schedule(_schedule_item);
				if (VERBOSE)
					log("deinitialize started");

				return true;

			default: break;
			}
			return false;
		}

		Result last_result()
		{
			switch (_state) {
			case INIT: return NONE;
			case DEINIT_SB_CONTROL: return PENDING;
			case DEINIT_SB_CONTROL_FAILED: return FAILED;
			case DEINIT_SB_CONTROL_SUCCEEDED: return SUCCEEDED;
			}
			ASSERT_NEVER_REACHED;
		}
};

class Vfs_tresor::Extend_operation : Noncopyable, Request_interface
{
	public:

		enum Result { NONE, SUCCEEDED, FAILED, PENDING };

	private:

		enum State { INIT, EXTEND_FT, EXTEND_FT_SUCCEEDED, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, COMPLETE };

		State _state { INIT };
		Result _last_result { NONE };
		bool _complete { };
		Number_of_blocks _num_blocks { };
		Constructible<Superblock_control::Extend_free_tree> _extend_ft { };
		Constructible<Superblock_control::Extend_vbd> _extend_vbd { };
		Schedule<Request_interface>::Item _schedule_item { this };

		/***********************
		 ** Request_interface **
		 ***********************/

		bool execute(Execute_attr const &attr) override
		{
			bool progress = false;
			switch (_state) {
			case EXTEND_FT:

				progress = attr.sb_control.execute(
					*_extend_ft, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor);

				if (_extend_ft->complete()) {
					if (_extend_ft->success()) {
						if (_complete) {
							_last_result = SUCCEEDED;
							_state = COMPLETE;
							if (VERBOSE)
								log("free-tree extension succeeded");
						} else
							_state = EXTEND_FT_SUCCEEDED;
					} else {
						_last_result = FAILED;
						_state = COMPLETE;
						if (VERBOSE)
							log("free-tree extension failed");
					}
					_extend_ft.destruct();
					attr.adapter.extend_fs_trigger_watch_response();
					progress = true;
				}
				break;

			case EXTEND_FT_SUCCEEDED:

				_extend_ft.construct(Superblock_control::Extend_free_tree::Attr{_num_blocks, _complete});
				_state = EXTEND_FT;
				progress = true;
				break;

			case EXTEND_VBD:

				progress = attr.sb_control.execute(
					*_extend_vbd, attr.vbd, attr.free_tree, attr.meta_tree, attr.block_io, attr.trust_anchor);

				if (_extend_vbd->complete()) {
					if (_extend_vbd->success()) {
						if (_complete) {
							_last_result = SUCCEEDED;
							_state = COMPLETE;
							if (VERBOSE)
								log("VBD extension succeeded");
						} else
							_state = EXTEND_VBD_SUCCEEDED;
					} else {
						_last_result = FAILED;
						_state = COMPLETE;
						if (VERBOSE)
							log("VBD extension failed");
					}
					_extend_vbd.destruct();
					progress = true;
				}
				break;

			case EXTEND_VBD_SUCCEEDED:

				_extend_vbd.construct(Superblock_control::Extend_vbd::Attr{_num_blocks, _complete});
				_state = EXTEND_VBD;
				progress = true;
				break;

			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		Scheduling_state scheduling_state() const override
		{
			switch (_state) {
			case EXTEND_FT: return CANNOT_YIELD;
			case EXTEND_FT_SUCCEEDED: return CAN_YIELD;
			case EXTEND_VBD: return CANNOT_YIELD;
			case EXTEND_VBD_SUCCEEDED: return CAN_YIELD;
			case COMPLETE: return REMOVE_FROM_SCHEDULE;
			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

		bool can_be_yielded_to() const override { return false; };

	public:

		bool try_start_extending_free_tree(Initialized_tresor_adapter_interface &adapter, Number_of_blocks num_blocks)
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_num_blocks = num_blocks;
				_last_result = PENDING;
				_complete = false;
				_extend_ft.construct(Superblock_control::Extend_free_tree::Attr{_num_blocks, _complete});
				_state = EXTEND_FT;
				adapter.add_to_schedule(_schedule_item);
				if (VERBOSE)
					log("free-tree extension started");

				return true;

			default: break;
			}
			return false;
		}

		bool try_start_extending_vbd(Initialized_tresor_adapter_interface &adapter, Number_of_blocks num_blocks)
		{
			switch (_state) {
			case INIT:
			case COMPLETE:

				_num_blocks = num_blocks;
				_last_result = PENDING;
				_complete = false;
				_extend_vbd.construct(Superblock_control::Extend_vbd::Attr{_num_blocks, _complete});
				_state = EXTEND_VBD;
				adapter.add_to_schedule(_schedule_item);
				if (VERBOSE)
					log("VBD extension started");

				return true;

			default: break;
			}
			return false;
		}

		Result last_result() { return _last_result; }
};

class Vfs_tresor::Tresor_adapter : Client_data_interface, Crypto_key_files_interface, Initialized_tresor_adapter_interface
{
	private:

		enum State { INIT_SB_CONTROL, NO_OPERATION, DATA_OPERATION, REKEY_OPERATION, EXTEND_OPERATION, DEINITIALIZE_OPERATION };

		enum { MAX_NUM_COMMANDS = 16 };

		struct Crypto_key
		{
			Key_id const key_id;
			Vfs::Vfs_handle &encrypt_file;
			Vfs::Vfs_handle &decrypt_file;
		};

		Vfs::Env &_vfs_env;
		bool const _verbose;
		Tresor::Path const _crypto_path;
		Tresor::Path const _block_io_path;
		Tresor::Path const _trust_anchor_path;
		Vfs::Vfs_handle &_block_io_file { open_file(_vfs_env, _block_io_path, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_crypto_add_key_file { open_file(_vfs_env, { _crypto_path, "/add_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_crypto_remove_key_file { open_file(_vfs_env, { _crypto_path, "/remove_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_ta_decrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_encrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_generate_key_file { open_file(_vfs_env, { _trust_anchor_path, "/generate_key" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_initialize_file { open_file(_vfs_env, { _trust_anchor_path, "/initialize" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_hash_file { open_file(_vfs_env, { _trust_anchor_path, "/hash" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Schedule<Request_interface> _schedule { };
		Tresor::Free_tree _free_tree { };
		Tresor::Virtual_block_device _vbd { };
		Superblock_control _sb_control { };
		Meta_tree _meta_tree { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Splitter _splitter { };
		Extend_file_system * _extend_fs_ptr  { };
		Extend_progress_file_system *_extend_progress_fs_ptr { };
		Rekey_file_system * _rekey_fs_ptr  { };
		Rekey_progress_file_system *_rekey_progress_fs_ptr { };
		Deinitialize_file_system *_deinit_fs_ptr  { };
		Constructible<Crypto_key> _crypto_keys[2] { };
		Superblock_control::Initialize *_init_sb_control_ptr { };
		Superblock::State _sb_state { Superblock::INVALID };
		Data_operation _data_operation { };
		Rekeying _rekeying { };
		Extend_operation _extending { };
		Deinitialize _deinitialize { };
		State _state { INIT_SB_CONTROL };

		/*
		 * Noncopyable
		 */
		Tresor_adapter(Tresor_adapter const &) = delete;
		Tresor_adapter &operator = (Tresor_adapter const &) = delete;

		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		bool _execute_schedule_items()
		{
			bool progress = false;
			_schedule.with_head([&] (Request_interface &head) {

				progress |= head.execute({*this, _splitter, _sb_control, *this, _vbd, _free_tree, _meta_tree, _block_io, _crypto, _trust_anchor});
				switch (head.scheduling_state()) {
				case Request_interface::REMOVE_FROM_SCHEDULE: _schedule.remove_head(); break;
				case Request_interface::CAN_YIELD:

					_schedule.try_yield_head([&] (Request_interface const &to_req) {
						return to_req.can_be_yielded_to(); });
					break;

				case Request_interface::CANNOT_YIELD: break;
				}
			});
			return progress;
		}

		bool _try_complete_init_sb_control()
		{
			if (!_init_sb_control_ptr)
				return true;

			while (_sb_control.execute(*_init_sb_control_ptr, _block_io, _crypto, _trust_anchor)) ;
			if (_init_sb_control_ptr->complete()) {

				ASSERT(_init_sb_control_ptr->success());
				destroy(_vfs_env.alloc(), _init_sb_control_ptr);
				_init_sb_control_ptr = nullptr;
				return true;
			}
			_wakeup_back_end_services();
			return false;
		}

		/********************************
		 ** Crypto_key_files_interface **
		 ********************************/

		void add_crypto_key(Key_id key_id) override
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (!key.constructed()) {
					key.construct(key_id,
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR),
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR)
					);
					return;
				}
			ASSERT_NEVER_REACHED;
		}

		void remove_crypto_key(Key_id key_id) override
		{
			Constructible<Crypto_key> &crypto_key = _crypto_key(key_id);
			_vfs_env.root_dir().close(&crypto_key->encrypt_file);
			_vfs_env.root_dir().close(&crypto_key->decrypt_file);
			crypto_key.destruct();
		}

		Vfs::Vfs_handle &encrypt_file(Key_id key_id) override { return _crypto_key(key_id)->encrypt_file; }
		Vfs::Vfs_handle &decrypt_file(Key_id key_id) override { return _crypto_key(key_id)->decrypt_file; }

		/***************************
		 ** Client_data_interface **
		 ***************************/

		void obtain_data(Obtain_data_attr const &attr) override
		{
			attr.out_blk = _splitter.source_buffer(attr.in_vba);
		}

		void supply_data(Supply_data_attr const &attr) override
		{
			_splitter.destination_buffer(attr.in_vba) = attr.in_blk;
		}

		/******************************************
		 ** Initialized_tresor_adapter_interface **
		 ******************************************/

		bool exceeds_data_file_range(addr_t start, size_t num_bytes) const override
		{
			addr_t last_byte = num_bytes ? start - 1 + num_bytes : start;
			addr_t last_file_byte = (_sb_control.max_vba() * BLOCK_SIZE) + BLOCK_SIZE - 1;
			return last_byte > last_file_byte;
		}

		size_t data_file_size() const override
		{
			return (_sb_control.max_vba() + 1) * BLOCK_SIZE;
		}

		void add_to_schedule(Schedule<Request_interface>::Item &item) override
		{
			_schedule.add_tail(item);
		}

		bool _choose_next_operation()
		{
			State state = _state;
			if (_data_operation.pending())
				_state = DATA_OPERATION;
			else
				_state = NO_OPERATION;

			return state != _state;
		}

		bool _execute_operations()
		{
			bool progress = false;
			switch (_state) {
			case INIT_SB_CONTROL:

				progress |= _sb_control.execute(*_init_sb_control_ptr, _block_io, _crypto, _trust_anchor) ;
				if (_init_sb_control_ptr->complete()) {

					ASSERT(_init_sb_control_ptr->success());
					destroy(_vfs_env.alloc(), _init_sb_control_ptr);
					_init_sb_control_ptr = nullptr;
					if (VERBOSE)
						log("init sb-control succeeded");

					progress |= _choose_next_operation();
				}
				break;

			case DATA_OPERATION:

				progress |= _data_operation.execute({_splitter, _sb_control, *this, _vbd, _free_tree, _meta_tree, _block_io, _crypto, _trust_anchor}) ;
				if (!_data_operation.pending())
					progress |= _choose_next_operation();
				break;

			case NO_OPERATION: progress |= _choose_next_operation(); break;
			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		bool execute() override
		{
			while (_execute_operations()) ;
			_wakeup_back_end_services();
			return false;
		}

		void extend_fs_trigger_watch_response() override;

		void extend_progress_fs_trigger_watch_response() override;

		void rekey_fs_trigger_watch_response() override;

		void rekey_progress_fs_trigger_watch_response() override;

		void deinit_fs_trigger_watch_response() override;

	public:

		Tresor_adapter(Vfs::Env &vfs_env, Xml_node const &config)
		:
			_vfs_env(vfs_env),
			_verbose(config.attribute_value("verbose", _verbose)),
			_crypto_path(config.attribute_value("crypto", Tresor::Path())),
			_block_io_path(config.attribute_value("block", Tresor::Path())),
			_trust_anchor_path(config.attribute_value("trust_anchor", Tresor::Path()))
		{
			_init_sb_control_ptr = new (_vfs_env.alloc()) Superblock_control::Initialize({_sb_state});
			if (VERBOSE)
				log("init sb-control started");
		}

		template <typename FUNC>
		void with_initialized_interface(FUNC && func)
		{
			execute();
			if (_state != INIT_SB_CONTROL)
				func(*this);
		}

		template <typename FUNC>
		void with_data_operation(FUNC && func)
		{
			execute();
			func(_data_operation);
			execute();
		}

		template <typename FUNC>
		void with_rekeying(FUNC && )
		{
			ASSERT_NEVER_REACHED;
		}

		template <typename FUNC>
		void with_extend_operation(FUNC && )
		{
			ASSERT_NEVER_REACHED;
		}

		template <typename FUNC>
		void with_deinitialize(FUNC && )
		{
			ASSERT_NEVER_REACHED;
		}

		void manage_extend_file_system(Extend_file_system &extend_fs)
		{
			ASSERT(!_extend_fs_ptr);
			_extend_fs_ptr = &extend_fs;
		}

		void dissolve_extend_file_system(Extend_file_system &extend_fs)
		{
			ASSERT(_extend_fs_ptr == &extend_fs);
			_extend_fs_ptr = nullptr;
		}

		void manage_extend_progress_file_system(Extend_progress_file_system &extend_progress_fs)
		{
			ASSERT(!_extend_progress_fs_ptr);
			_extend_progress_fs_ptr = &extend_progress_fs;
		}

		void dissolve_extend_progress_file_system(Extend_progress_file_system &extend_progress_fs)
		{
			ASSERT(_extend_progress_fs_ptr == &extend_progress_fs);
			_extend_progress_fs_ptr = nullptr;
		}

		void manage_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			ASSERT(!_rekey_fs_ptr);
			_rekey_fs_ptr = &rekey_fs;
		}

		void dissolve_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			ASSERT(_rekey_fs_ptr == &rekey_fs);
			_rekey_fs_ptr = nullptr;
		}

		void manage_rekey_progress_file_system(Rekey_progress_file_system &rekey_progress_fs)
		{
			ASSERT(!_rekey_progress_fs_ptr);
			_rekey_progress_fs_ptr = &rekey_progress_fs;
		}

		void dissolve_rekey_progress_file_system(Rekey_progress_file_system &rekey_progress_fs)
		{
			ASSERT(_rekey_progress_fs_ptr == &rekey_progress_fs);
			_rekey_progress_fs_ptr = nullptr;
		}

		void manage_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			ASSERT(!_deinit_fs_ptr);
			_deinit_fs_ptr = &deinit_fs;
		}

		void dissolve_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			ASSERT(_deinit_fs_ptr == &deinit_fs);
			_deinit_fs_ptr = nullptr;
		}
};


class Vfs_tresor::Data_operation_system : public Single_file_system
{
	private:

		Tresor_adapter &_adapter;

	public:

		class Vfs_handle : Noncopyable, public Single_vfs_handle
		{
			private:

				Tresor_adapter &_adapter;

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Allocator &alloc, Tresor_adapter &adapter)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _adapter(adapter)
				{ }

				/***********************
				 ** Single_vfs_handle **
				 ***********************/

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					Read_result result = READ_QUEUED;
					_adapter.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.file_read(seek(), dst)) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							out_count = dst.num_bytes;
							result = READ_OK;
							break;

						case Data_operation::FAILED: result = READ_ERR_IO; break;
						};
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					Write_result result = WRITE_ERR_WOULD_BLOCK;
					_adapter.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.file_write(seek(), src)) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							out_count = src.num_bytes;
							result = WRITE_OK;
							break;

						case Data_operation::FAILED: result = WRITE_ERR_IO; break;
						};
					});
					return result;
				}

				Sync_result sync() override
				{
					Sync_result result = SYNC_QUEUED;
					_adapter.with_data_operation([&] (Data_operation &data_operation) {

						switch (data_operation.file_sync()) {
						case Data_operation::PENDING: break;
						case Data_operation::SUCCEEDED:

							result = SYNC_OK;
							break;

						case Data_operation::FAILED: result = SYNC_ERR_INVALID; break;
						};
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

		Data_operation_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, type_name(), Node_rwx::rw(), Xml_node("<data/>")), _adapter(adapter)
		{ }

		/************************
		 ** Single_file_system **
		 ************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = STAT_ERR_NO_ENTRY;
			_adapter.with_initialized_interface([&] (Initialized_tresor_adapter_interface &adapter) {
				result = Single_file_system::stat(path, out);
				out.size = adapter.data_file_size();
			});
			return result;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override { return FTRUNCATE_OK; }

		Open_result open(char const *path, unsigned, Vfs::Vfs_handle **out_handle, Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
			return OPEN_OK;
		}

		static char const *type_name() { return "data"; }

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Extend_file_system : public Vfs::Single_file_system
{
	private:

		using Registered_watch_handle = Registered<Vfs_watch_handle>;
		using Watch_handle_registry = Registry<Registered_watch_handle>;
		using Content_string = String<11>;

		Watch_handle_registry _handle_registry { };
		Tresor_adapter &_adapter;

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				enum Tree { VBD, FREE_TREE };

				Tresor_adapter &_adapter;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

			Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
			           Allocator &alloc, Tresor_adapter &adapter)
			:
				Single_vfs_handle(dir_service, file_io_service, alloc, 0), _adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;
				Read_result result = READ_QUEUED;
				_adapter.with_extend_operation([&] (Initialized_tresor_adapter_interface &adapter, Extend_operation &extend_operation) {

					if (seek() == dst.num_bytes) {
						result = READ_OK;
						return;
					}
					if (seek() || dst.num_bytes < Content_string::capacity()) {
						result = READ_ERR_IO;
						if (VERBOSE)
							log("malformed read request at extend file");
						return;
					}
					adapter.execute();
					switch (extend_operation.last_result()) {
					case Extend_operation::NONE: result = _read_ok("none", dst, out_count); break;
					case Extend_operation::SUCCEEDED: result = _read_ok("successful", dst, out_count); break;
					case Extend_operation::FAILED: result = _read_ok("failed", dst, out_count); break;
					case Extend_operation::PENDING: break;
					}
				});
				return result;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				out_count = 0;
				Write_result result = WRITE_ERR_WOULD_BLOCK;
				_adapter.with_extend_operation([&] (Initialized_tresor_adapter_interface &adapter, Extend_operation &extend_operation) {

					char tree_arg[16];
					Arg_string::find_arg(src.start, "tree").string(tree_arg, sizeof(tree_arg), "-");
					unsigned long blocks_arg = Arg_string::find_arg(src.start, "blocks").ulong_value(0);
					if (seek() || !blocks_arg) {
						result = WRITE_ERR_IO;
						if (VERBOSE)
							log("malformed write request at extend file");
					}
					adapter.execute();
					if (!strcmp("ft", tree_arg, 2)) {

						if (!extend_operation.try_start_extending_free_tree(adapter, blocks_arg)) {
							result = WRITE_ERR_IO;
							if (VERBOSE)
								log("failed to start extend_operation free tree");
							return;
						}

					} else if (!strcmp("vbd", tree_arg, 3)) {

						if (!extend_operation.try_start_extending_vbd(adapter, blocks_arg)) {
							result = WRITE_ERR_IO;
							if (VERBOSE)
								log("failed to start extend_operation VBD");
							return;
						}

					} else {

						result = WRITE_ERR_IO;
						if (VERBOSE)
							log("malformed tree argument while writing extend file");
						return;
					}
					out_count = src.num_bytes;
					result = WRITE_OK;
				});
				return result;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Extend_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<extend/>")),
			_adapter(adapter)
		{
			_adapter.manage_extend_file_system(*this);
		}

		static char const *type_name() { return "extend"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity() - 1;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Extend_progress_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Tresor_adapter &_adapter;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Tresor_adapter &_adapter;

			Vfs_handle(Directory_service &ds,
			           File_io_service &fs,
			           Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &,
			                 size_t               &) override
			{
				ASSERT_NEVER_REACHED;
			}

			Write_result write(Const_byte_range_ptr const &,
			                   size_t                     &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Extend_progress_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<extend_progress/>")),
			_adapter(adapter)
		{
			_adapter.manage_extend_progress_file_system(*this);
		}

		static char const *type_name() { return "extend_progress"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity() - 1;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Rekey_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Tresor_adapter &_adapter;

		using Content_string = String<11>;

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				Tresor_adapter &_adapter;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

			Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
			           Allocator &alloc, Tresor_adapter &adapter)
			:
				Single_vfs_handle(dir_service, file_io_service, alloc, 0), _adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;
				Read_result result = READ_QUEUED;
				_adapter.with_rekeying([&] (Initialized_tresor_adapter_interface &adapter, Rekeying &rekeying) {

					if (seek() == dst.num_bytes) {
						result = READ_OK;
						return;
					}
					if (seek() || dst.num_bytes < Content_string::capacity()) {
						result = READ_ERR_IO;
						if (VERBOSE)
							log("malformed read request at rekey file");
						return;
					}
					adapter.execute();
					switch (rekeying.last_result()) {
					case Rekeying::NONE: result = _read_ok("none", dst, out_count); break;
					case Rekeying::SUCCEEDED: result = _read_ok("successful", dst, out_count); break;
					case Rekeying::FAILED: result = _read_ok("failed", dst, out_count); break;
					case Rekeying::PENDING: break;
					}
				});
				return result;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				out_count = 0;
				Write_result result = WRITE_ERR_IO;
				_adapter.with_rekeying([&] (Initialized_tresor_adapter_interface &adapter, Rekeying &rekeying) {

					bool start_rekeying_arg { false };
					Genode::ascii_to(src.start, start_rekeying_arg);
					if (seek() || !start_rekeying_arg) {
						result = WRITE_ERR_IO;
						if (VERBOSE)
							log("malformed write request at rekey file");
						return;
					}
					adapter.execute();
					if (!rekeying.try_start(adapter)) {
						result = WRITE_ERR_IO;
						if (VERBOSE)
							log("failed to start rekeying");
						return;
					}
					out_count = src.num_bytes;
				});
				return result;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Rekey_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<rekey/>")),
			_adapter(adapter)
		{
			_adapter.manage_rekey_file_system(*this);
		}

		static char const *type_name() { return "rekey"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::size();
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Rekey_progress_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Tresor_adapter &_adapter;

		using Content_string = String<32>;

		static file_size copy_content(Content_string const &content,
		                              char *dst, size_t const count)
		{
			copy_cstring(dst, content.string(), count);
			size_t const length_without_nul = content.length() - 1;
			return count > length_without_nul - 1 ? length_without_nul
			                                      : count;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Tresor_adapter &_adapter;

			Vfs_handle(Directory_service &ds,
			           File_io_service &fs,
			           Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &,
			                 size_t               &) override
			{
				ASSERT_NEVER_REACHED;
			}

			Write_result write(Const_byte_range_ptr const &,
			                   size_t                     &) override
			{
				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Rekey_progress_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<rekey_progress/>")),
			_adapter(adapter)
		{
			_adapter.manage_rekey_progress_file_system(*this);
		}

		static char const *type_name() { return "rekey_progress"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity() - 1;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Deinitialize_file_system : public Vfs::Single_file_system
{
	private:

		using Registered_watch_handle = Registered<Vfs_watch_handle>;
		using Watch_handle_registry = Registry<Registered_watch_handle>;
		using Content_string = String<11>;

		Watch_handle_registry _handle_registry { };
		Tresor_adapter &_adapter;

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				Tresor_adapter &_adapter;

				static Read_result _read_ok(Content_string const &content, Byte_range_ptr const &dst, size_t &out_count)
				{
					copy_cstring(dst.start, content.string(), dst.num_bytes);
					out_count = dst.num_bytes;
					return READ_OK;
				}

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service, Allocator &alloc, Tresor_adapter &adapter)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0), _adapter(adapter)
				{ }

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					out_count = 0;
					Read_result result = READ_QUEUED;
					_adapter.with_deinitialize([&] (Initialized_tresor_adapter_interface &adapter, Deinitialize &deinitialize) {

						if (seek() == dst.num_bytes) {
							result = READ_OK;
							return;
						}
						if (seek() || dst.num_bytes < Content_string::capacity()) {
							result = READ_ERR_IO;
							if (VERBOSE)
								log("malformed read request at deinitialize file");
							return;
						}
						adapter.execute();
						switch (deinitialize.last_result()) {
						case Deinitialize::NONE: result = _read_ok("none", dst, out_count); break;
						case Deinitialize::SUCCEEDED: result = _read_ok("successful", dst, out_count); break;
						case Deinitialize::FAILED: result = _read_ok("failed", dst, out_count); break;
						case Deinitialize::PENDING: break;
						}
					});
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					out_count = 0;
					Write_result result = WRITE_ERR_IO;
					_adapter.with_deinitialize([&] (Initialized_tresor_adapter_interface &adapter, Deinitialize &deinitialize) {

						bool start_deinitialize { false };
						Genode::ascii_to(src.start, start_deinitialize);
						if (seek() || !start_deinitialize) {
							if (VERBOSE)
								log("malformed write request at deinitialize file");
							return;
						}
						adapter.execute();
						if (!deinitialize.try_start(adapter)) {
							if (VERBOSE)
								log("failed to start deinitialize");
							return;
						}
						out_count = src.num_bytes;
						result = WRITE_OK;
					});
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

	public:

		Deinitialize_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(
				Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<deinitialize/>")),
			_adapter(adapter)
		{
			_adapter.manage_deinit_file_system(*this);
		}

		static char const *type_name() { return "deinitialize"; }

		char const *type() override { return type_name(); }

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_single_file(path))
				return WATCH_ERR_UNACCESSIBLE;

			try {
				*handle = new (alloc)
					Registered_watch_handle(_handle_registry, *this, alloc);

				return WATCH_OK;
			}
			catch (Out_of_ram)  { return WATCH_ERR_OUT_OF_RAM;  }
			catch (Out_of_caps) { return WATCH_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_watch_handle *handle) override
		{
			destroy(handle->alloc(),
			        static_cast<Registered_watch_handle *>(handle));
		}

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = Content_string::capacity() - 1;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs_tresor::Current_local_factory : File_system_factory
{
	Data_operation_system _data_fs;

	Current_local_factory(Vfs::Env &, Tresor_adapter &adapter) : _data_fs(adapter) { }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Data_operation_system::type_name()))
			return &_data_fs;

		return nullptr;
	}
};


class Vfs_tresor::Current_file_system : private Current_local_factory, public Vfs::Dir_file_system
{
	private:

		typedef String<128> Config;

		static Config _config()
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] ()
			{
				xml.attribute("name", String<16>("current"));
				xml.node("data", [&] () {
					xml.attribute("readonly", false);
				});
			});

			return Config(Cstring(buf));
		}

	public:

		Current_file_system(Vfs::Env &vfs_env, Tresor_adapter &adapter)
		:
			Current_local_factory(vfs_env, adapter),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config().string()), *this)
		{ }

		static char const *type_name() { return "current"; }

		/**************************
		 ** Vfs::Dir_file_system **
		 **************************/

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Control_local_factory : public File_system_factory
{
	private:

		Tresor_adapter &_adapter;
		Rekey_file_system _rekeying_fs;
		Rekey_progress_file_system _rekeying_progress_fs;
		Deinitialize_file_system _deinitialize_fs;
		Extend_file_system _extend_fs;
		Extend_progress_file_system _extend_progress_fs;

	public:

		Control_local_factory(Vfs::Env &, Xml_node, Tresor_adapter &adapter)
		:
			_adapter(adapter), _rekeying_fs(adapter), _rekeying_progress_fs(adapter),
			_deinitialize_fs(adapter), _extend_fs(adapter), _extend_progress_fs(adapter)
		{ }

		~Control_local_factory()
		{
			_adapter.dissolve_rekey_file_system(_rekeying_fs);
			_adapter.dissolve_rekey_progress_file_system(_rekeying_progress_fs);
			_adapter.dissolve_deinit_file_system(_deinitialize_fs);
			_adapter.dissolve_extend_file_system(_extend_fs);
			_adapter.dissolve_extend_progress_file_system(_extend_progress_fs);
		}

		Vfs::File_system *create(Vfs::Env&, Xml_node node) override
		{
			if (node.has_type(Rekey_file_system::type_name()))
				return &_rekeying_fs;

			if (node.has_type(Rekey_progress_file_system::type_name()))
				return &_rekeying_progress_fs;

			if (node.has_type(Deinitialize_file_system::type_name()))
				return &_deinitialize_fs;

			if (node.has_type(Extend_file_system::type_name()))
				return &_extend_fs;

			if (node.has_type(Extend_progress_file_system::type_name()))
				return &_extend_progress_fs;

			return nullptr;
		}
};


class Vfs_tresor::Control_file_system : Control_local_factory, public Vfs::Dir_file_system
{
	private:

		typedef String<256> Config;

		static Config _config()
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "control");
				xml.node("rekey", [&] () { });
				xml.node("rekey_progress", [&] () { });
				xml.node("extend", [&] () { });
				xml.node("extend_progress", [&] () { });
				xml.node("deinitialize", [&] () { });
			});
			return Config(Cstring(buf));
		}

	public:

		Control_file_system(Vfs::Env         &vfs_env,
		                    Xml_node  node,
		                    Tresor_adapter          &tresor)
		:
			Control_local_factory(vfs_env, node, tresor),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config().string()), *this)
		{ }

		static char const *type_name() { return "control"; }

		/**************************
		 ** Vfs::Dir_file_system **
		 **************************/

		char const *type() override { return type_name(); }
};


class Vfs_tresor::Local_factory : public File_system_factory
{
	private:

		Tresor_adapter  &_adapter;
		Current_file_system _current_fs;
		Control_file_system _control_fs;

	public:

		Local_factory(Vfs::Env &env, Xml_node config, Tresor_adapter &adapter)
		:
			_adapter(adapter), _current_fs(env, adapter), _control_fs(env, config, adapter)
		{ }

		/*************************
		 ** File_system_factory **
		 *************************/

		Vfs::File_system *create(Vfs::Env&, Xml_node node) override
		{
			if (node.has_type(Current_file_system::type_name()))
				return &_current_fs;

			if (node.has_type(Control_file_system::type_name()))
				return &_control_fs;

			return nullptr;
		}
};


class Vfs_tresor::File_system : Local_factory, public Vfs::Dir_file_system
{
	private:

		using Config = String<256>;

		Tresor_adapter &_adapter;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };
			Xml_generator xml(buf, sizeof(buf), "dir", [&] ()
			{
				xml.attribute("name", node.attribute_value("name", String<64>("tresor")));
				xml.node("control", [&] () { });
				xml.node("current", [&] () { });
			});
			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Xml_node node, Tresor_adapter &adapter)
		:
			Local_factory(vfs_env, node, adapter),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()), *this),
			_adapter(adapter)
		{ }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	class Factory : public Vfs::File_system_factory
	{
		private:

			Allocator *_alloc_ptr { };
			Vfs_tresor::Tresor_adapter *_adapter_ptr { };

			/*************************
			 ** File_system_factory **
			 *************************/

			Vfs::File_system *create(Vfs::Env &env, Xml_node node) override
			{
				try {
					if (!_adapter_ptr) {
						_alloc_ptr = &env.alloc();
						_adapter_ptr = new (*_alloc_ptr) Vfs_tresor::Tresor_adapter { env, node };
					}
					return new (env.alloc()) Vfs_tresor::File_system(env, node, *_adapter_ptr);

				} catch (...) { error("could not create 'tresor_fs' "); }
				return nullptr;
			}

		public:

			~Factory()
			{
				if (_adapter_ptr)
					destroy(_alloc_ptr, _adapter_ptr);
			}
	};

	static Factory factory { };
	return &factory;
}


void Vfs_tresor::Tresor_adapter::extend_fs_trigger_watch_response()
{
	if (_extend_fs_ptr)
		_extend_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::extend_progress_fs_trigger_watch_response()
{
	if (_extend_progress_fs_ptr)
		_extend_progress_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::rekey_fs_trigger_watch_response()
{
	if (_rekey_fs_ptr)
		_rekey_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::rekey_progress_fs_trigger_watch_response()
{
	if (_rekey_progress_fs_ptr)
		_rekey_progress_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::deinit_fs_trigger_watch_response()
{
	if (_deinit_fs_ptr)
		_deinit_fs_ptr->trigger_watch_response();
}
