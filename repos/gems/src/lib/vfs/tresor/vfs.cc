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

#include "splitter.h"

namespace Vfs_tresor {

	enum { VERBOSE = 1 };

	using namespace Vfs;
	using namespace Genode;
	using namespace Tresor;

	template <typename> class Schedule;
	class Schedule_item;
	class Data_file_system;
	class Extend_file_system;
	class Extend_progress_file_system;
	class Rekey_file_system;
	class Rekey_progress_file_system;
	class Deinitialize_file_system;
	class Create_snapshot_file_system;
	class Discard_snapshot_file_system;
	class Control_local_factory;
	class Control_file_system;
	class Snapshot_local_factory;
	class Snapshot_file_system;
	class Snapshots_local_factory;
	class Snapshots_file_system;
	class Local_factory;
	class File_system;
	class Tresor_adapter;
	class Crypto_key;
}

struct Vfs_tresor::Crypto_key
{
	Key_id const key_id;
	Vfs::Vfs_handle &encrypt_file;
	Vfs::Vfs_handle &decrypt_file;
};

template <typename T>
class Vfs_tresor::Schedule : Noncopyable
{
	private:

		T *_tail { };
		List<T> _list { };

	public:

		using Item = List<T>::Element;

		void add_tail(T &request)
		{
			_list.insert(&request, _tail);
			_tail = &request;
		}

		bool empty() const { return !_list.first(); }

		template <typename FN>
		void with_head(FN && fn)
		{
			if (_list.first())
				fn(*_list.first());
		}

		void remove_head()
		{
			T *head = _list.first();
			if (!head)
				return;

			_list.remove(head);
			if (_tail == head)
				_tail = _list.first();
		}

		template <typename CAN_YIELD_TO_FN>
		void try_yield_head(CAN_YIELD_TO_FN && can_yield_to)
		{
			T *head = _list.first();
			if (!head)
				return;

			T *next = head->Item::_next;
			if (!next || !can_yield_to(*next))
				return;

			remove_head();
			_list.insert(head, next);
		}
};


struct Vfs_tresor::Schedule_item : private Schedule<Schedule_item>::Item
{
	friend class Schedule<Schedule_item>;
	friend class List<Schedule_item>;

	enum State { COMPLETE, CAN_YIELD, CANNOT_YIELD };

	virtual bool execute() = 0;

	virtual State state() const = 0;

	virtual bool can_be_yielded_to() const = 0;

	virtual ~Schedule_item() { }
};


class Vfs_tresor::Tresor_adapter : public Client_data_interface, public Crypto_key_files_interface
{
	private:

		enum { MAX_NUM_COMMANDS = 16 };

		Vfs::Env &_vfs_env;
		bool const _verbose;
		bool const _debug;
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
		Schedule<Schedule_item> _schedule { };
		Tresor::Free_tree _free_tree { };
		Tresor::Virtual_block_device _vbd { };
		Superblock_control _sb_control { };
		Meta_tree _meta_tree { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Splitter _splitter { };
		Snapshots_file_system *_snapshots_fs_ptr { };
		Extend_file_system * _extend_fs_ptr  { };
		Extend_progress_file_system *_extend_progress_fs_ptr { };
		Rekey_file_system * _rekey_fs_ptr  { };
		Rekey_progress_file_system *_rekey_progress_fs_ptr { };
		Deinitialize_file_system *_deinit_fs_ptr  { };
		Constructible<Crypto_key> _crypto_keys[2] { };

		/*
		 * Noncopyable
		 */
		Tresor_adapter(Tresor_adapter const &) = delete;
		Tresor_adapter &operator = (Tresor_adapter const &) = delete;

		void _snapshots_fs_update_snapshot_registry();

		void _extend_fs_trigger_watch_response();

		void _extend_progress_fs_trigger_watch_response();

		void _rekey_fs_trigger_watch_response();

		void _rekey_progress_fs_trigger_watch_response();

		void _deinit_fs_trigger_watch_response();

		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }


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

	public:

		Tresor_adapter(Vfs::Env &vfs_env, Xml_node const &config)
		:
			_vfs_env(vfs_env),
			_verbose(config.attribute_value("verbose", _verbose)),
			_debug(config.attribute_value("debug", _debug)),
			_crypto_path(config.attribute_value("crypto", Tresor::Path())),
			_block_io_path(config.attribute_value("block", Tresor::Path())),
			_trust_anchor_path(config.attribute_value("trust_anchor", Tresor::Path()))
		{ }

		addr_t last_byte_of_data_file() const
		{
			return (_sb_control.max_vba() * BLOCK_SIZE) + BLOCK_SIZE - 1;
		}

		size_t size_of_data_file() const
		{
			return (_sb_control.max_vba() + 1) * BLOCK_SIZE;
		}

		void add_to_schedule(Schedule_item &item)
		{
			_schedule.add_tail(item);
		}

		bool _execute_schedule_items()
		{
			bool progress = false;
			_schedule.with_head([&] (Schedule_item &head) {

				progress |= head.execute();
				switch (head.state()) {
				case Schedule_item::COMPLETE: _schedule.remove_head(); break;
				case Schedule_item::CAN_YIELD:

					_schedule.try_yield_head([&] (Schedule_item const &item) {
						return item.can_be_yielded_to(); });
					break;

				case Schedule_item::CANNOT_YIELD: break;
				}
			});
			return progress;
		}

		bool execute()
		{
			bool progress = _execute_schedule_items();
			_wakeup_back_end_services();
			return progress;
		}

		void snapshots_info(Tresor::Snapshots_info &info)
		{
			info = _sb_control.snapshots_info();
			execute();
		}


		/***********************************************************
		 ** Manange/Disolve interface needed for FS notifications **
		 ***********************************************************/

		void manage_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			ASSERT(!_snapshots_fs_ptr);
			_snapshots_fs_ptr = &snapshots_fs;
		}

		void dissolve_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			ASSERT(_snapshots_fs_ptr == &snapshots_fs);
			_snapshots_fs_ptr = nullptr;
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


class Vfs_tresor::Data_file_system : public Single_file_system
{
	private:

		Tresor_adapter &_adapter;
		Generation const _generation;

		using Read_result = Vfs::File_io_service::Read_result;
		using Sync_result = Vfs::File_io_service::Sync_result;
		using Write_result = Vfs::File_io_service::Write_result;

	public:

		class Vfs_handle : Noncopyable, public Single_vfs_handle, Schedule_item
		{
			private:

				enum State { INIT, READ, WRITE, SYNC };

				State _state { INIT };
				Tresor_adapter &_adapter;
				Generation const _generation { };
				Constructible<Splitter::Write> _write { };
				Constructible<Splitter::Read> _read { };
				Constructible<Superblock_control::Synchronize> _sync { };

				bool _exceeds_file_range(addr_t start, size_t num_bytes) const
				{
					addr_t last_byte = num_bytes ? start - 1 + num_bytes : start;
					return last_byte > _adapter.last_byte_of_data_file();
				}

				Sync_result _execute_sync()
				{
					Sync_result result;
					while (_adapter.execute()) ;
					if (_sync->complete()) {
						if (_sync->success())
							result = Sync_result::SYNC_OK;
						else
							result = Sync_result::SYNC_ERR_INVALID;
						_sync.destruct();
						_state = INIT;
					} else
						result = Sync_result::SYNC_QUEUED;
					return result;
				}

				Write_result _execute_write(size_t dst_num_bytes, size_t &out_count)
				{
					Write_result result;
					while (_adapter.execute()) ;
					if (_write->complete()) {
						if (_write->success()) {
							out_count = dst_num_bytes;
							result = Write_result::WRITE_OK;
						} else {
							out_count = 0;
							result = Write_result::WRITE_ERR_IO;
						}
						_write.destruct();
						_state = INIT;
					} else {
						out_count = 0;
						result = Write_result::WRITE_ERR_WOULD_BLOCK;
					}
					return result;
				}

				Read_result _execute_read(size_t dst_num_bytes, size_t &out_count)
				{
					Read_result result;
					while (_adapter.execute()) ;
					if (_read->complete()) {
						if (_read->success()) {
							out_count = dst_num_bytes;
							result = Read_result::READ_OK;
						} else {
							out_count = 0;
							result = Read_result::READ_ERR_IO;
						}
						_read.destruct();
						_state = INIT;
					} else {
						out_count = 0;
						result = Read_result::READ_QUEUED;
					}
					return result;
				}

				/*******************
				 ** Schedule_item **
				 *******************/

				bool execute() override { ASSERT_NEVER_REACHED; }

				Schedule_item::State state() const override { ASSERT_NEVER_REACHED; }

				bool can_be_yielded_to() const override { ASSERT_NEVER_REACHED; }

			public:

				Vfs_handle(Directory_service &dir_service, File_io_service &file_io_service,
				           Genode::Allocator &alloc, Tresor_adapter &adapter, Generation generation)
				:
					Single_vfs_handle(dir_service, file_io_service, alloc, 0),
					_adapter(adapter), _generation(generation)
				{ }

				Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
				{
					Read_result result;
					switch (_state) {
					case INIT:
					{
						if (_exceeds_file_range(seek(), dst.num_bytes)) {
							out_count = 0;
							result = Read_result::READ_ERR_IO;
							break;
						}
						_read.construct(Splitter::Read::Attr{seek(), _generation, dst.start, dst.num_bytes});
						_state = READ;
						_adapter.add_to_schedule(*this);
						result = _execute_read(dst.num_bytes, out_count);
						break;
					}
					case READ: result = _execute_read(dst.num_bytes, out_count); break;
					default:

						out_count = 0;
						result = Read_result::READ_QUEUED;
						break;
					}
					return result;
				}

				Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
				{
					Write_result result;
					switch (_state) {
					case INIT:
					{
						if (_exceeds_file_range(seek(), src.num_bytes)) {
							out_count = 0;
							result = Write_result::WRITE_ERR_IO;
							break;
						}
						_write.construct(Splitter::Write::Attr{seek(), _generation, src.start, src.num_bytes});
						_state = WRITE;
						_adapter.add_to_schedule(*this);
						result = _execute_write(src.num_bytes, out_count);
						break;
					}
					case WRITE: result = _execute_write(src.num_bytes, out_count); break;
					default:

						out_count = 0;
						result = Write_result::WRITE_ERR_WOULD_BLOCK;
						break;
					}
					return result;
				}

				Sync_result sync() override
				{
					Sync_result result;
					switch (_state) {
					case INIT:
					{
						_sync.construct(Superblock_control::Synchronize::Attr{});
						_state = SYNC;
						_adapter.add_to_schedule(*this);
						result = _execute_sync();
						break;
					}
					case SYNC: result = _execute_sync(); break;
					default: result = Sync_result::SYNC_QUEUED; break;
					}
					return result;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return true; }
		};

		Data_file_system(Tresor_adapter &adapter, Generation generation)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, type_name(), Node_rwx::rw(), Xml_node("<data/>")),
			_adapter(adapter), _generation(generation)
		{ }

		~Data_file_system()
		{
			/* XXX sync on close */
			/* XXX invalidate any still pending request */
		}

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _adapter.size_of_data_file();
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }

		/***************************
		 ** File-system interface **
		 ***************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle =
				new (alloc) Vfs_handle(*this, *this, alloc, _adapter, _generation);

			return OPEN_OK;
		}

		static char const *type_name() { return "data"; }
		char const *type() override { return type_name(); }
};


class Vfs_tresor::Extend_file_system : public Vfs::Single_file_system
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
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
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
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
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
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
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
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
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


class Vfs_tresor::Rekey_file_system : public Vfs::Single_file_system
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

			/* store VBA in case the handle is kept open */
			Virtual_block_address _last_rekeying_vba;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter),
				_last_rekeying_vba(0)
			{
				ASSERT_NEVER_REACHED;
			}

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
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
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
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
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
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
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
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


class Vfs_tresor::Deinitialize_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Tresor_adapter &_adapter;

		using Content_string = String<32>;

		static Content_string content_string(Tresor_adapter const &)
		{
			ASSERT_NEVER_REACHED;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Tresor_adapter &_adapter;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Deinitialize_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<deinitialize/>")),
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
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = content_string(_adapter).length() - 1;
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Create_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Tresor_adapter &_adapter;

		struct Vfs_handle : Single_vfs_handle
		{
			Tresor_adapter &_adapter;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Create_snapshot_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<create_snapshot/>")),
			_adapter(adapter)
		{ }

		static char const *type_name() { return "create_snapshot"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


class Vfs_tresor::Discard_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Tresor_adapter &_adapter;

		struct Vfs_handle : Single_vfs_handle
		{
			Tresor_adapter &_adapter;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Tresor_adapter &adapter)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_adapter(adapter)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &,
			                   size_t &) override
			{
				ASSERT_NEVER_REACHED;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Discard_snapshot_file_system(Tresor_adapter &adapter)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<discard_snapshot/>")),
			_adapter(adapter)
		{ }

		static char const *type_name() { return "discard_snapshot"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Vfs_handle(*this, *this, alloc, _adapter);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs_tresor::Snapshot_local_factory : File_system_factory
{
	Data_file_system _block_fs;

	Snapshot_local_factory(Vfs::Env & /* env */,
	                       Tresor_adapter &tresor,
	                       Generation snap_gen)
	: _block_fs(tresor, snap_gen) { }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Data_file_system::type_name()))
			return &_block_fs;

		return nullptr;
	}
};


class Vfs_tresor::Snapshot_file_system : private Snapshot_local_factory,
                                         public Vfs::Dir_file_system
{
	private:

		Generation _snap_gen;

		typedef String<128> Config;

		static Config _config(Generation snap_gen, bool readonly)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {

				xml.attribute("name", !readonly ? String<16>("current") : String<16>(snap_gen));
				xml.node("data", [&] () {
					xml.attribute("readonly", readonly);
				});
			});

			return Config(Cstring(buf));
		}

	public:

		Snapshot_file_system(Vfs::Env &vfs_env,
		                    Tresor_adapter &tresor,
		                    Generation snap_gen,
		                    bool readonly = false)
		:
			Snapshot_local_factory(vfs_env, tresor, snap_gen),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(snap_gen, readonly).string()), *this),
			_snap_gen(snap_gen)
		{ }

		static char const *type_name() { return "snapshot"; }

		char const *type() override { return type_name(); }

		Generation snap_gen() const { return _snap_gen; }
};


class Vfs_tresor::Snapshots_file_system : public Vfs::File_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Vfs::Env &_vfs_env;

		bool _root_dir(char const *path) { return strcmp(path, "/snapshots") == 0; }
		bool _top_dir(char const *path) { return strcmp(path, "/") == 0; }

		struct Snapshot_registry
		{
			Genode::Allocator                                          &_alloc;
			Tresor_adapter                                             &_adapter;
			Snapshots_file_system                                      &_snapshots_fs;
			uint32_t                                                    _number_of_snapshots { 0 };
			Genode::Registry<Genode::Registered<Snapshot_file_system>>  _registry            { };

			struct Invalid_index : Genode::Exception { };
			struct Invalid_path  : Genode::Exception { };



			Snapshot_registry(Genode::Allocator     &alloc,
			                  Tresor_adapter               &adapter,
			                  Snapshots_file_system &snapshots_fs)
			:
				_alloc(alloc), _adapter(adapter), _snapshots_fs(snapshots_fs)
			{ }

			void update(Vfs::Env &vfs_env);

			uint32_t number_of_snapshots() const { return _number_of_snapshots; }

			Snapshot_file_system const &by_index(uint64_t idx) const
			{
				uint64_t i = 0;
				Snapshot_file_system const *fsp { nullptr };
				auto lookup = [&] (Snapshot_file_system const &fs) {
					if (i == idx) {
						fsp = &fs;
					}
					++i;
				};
				_registry.for_each(lookup);
				if (fsp == nullptr) {
					throw Invalid_index();
				}
				return *fsp;
			}

			Snapshot_file_system &_by_gen(Generation snap_gen)
			{
				Snapshot_file_system *fsp { nullptr };
				auto lookup = [&] (Snapshot_file_system &fs) {
					if (fs.snap_gen() == snap_gen) {
						fsp = &fs;
					}
				};
				_registry.for_each(lookup);
				if (fsp == nullptr)
					throw Invalid_path();

				return *fsp;
			}

			Snapshot_file_system &by_path(char const *path)
			{
				if (!path)
					throw Invalid_path();

				if (path[0] == '/')
					path++;

				Generation snap_gen { INVALID_GENERATION };
				Genode::ascii_to(path, snap_gen);
				return _by_gen(snap_gen);
			}
		};

	public:

		void update_snapshot_registry()
		{
			_snap_reg.update(_vfs_env);
		}

		void trigger_watch_response()
		{
			_handle_registry.for_each([this] (Registered_watch_handle &handle) {
				handle.watch_response(); });
		}

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override
		{
			if (!_root_dir(path))
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

		struct Snap_vfs_handle : Vfs::Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(Byte_range_ptr const &, size_t &out_count) = 0;

			virtual Write_result write(Const_byte_range_ptr const &, size_t &out_count) = 0;

			virtual Sync_result sync()
			{
				return SYNC_OK;
			}

			virtual bool read_ready() const = 0;
		};


		struct Dir_vfs_handle : Snap_vfs_handle
		{
			Snapshot_registry const &_snap_reg;

			bool const _root_dir { false };

			Read_result _query_snapshots(size_t  index,
			                             size_t &out_count,
			                             Dirent &out)
			{
				if (index >= _snap_reg.number_of_snapshots()) {
					out_count = sizeof(Dirent);
					out.type = Dirent_type::END;
					return READ_OK;
				}

				try {
					Snapshot_file_system const &fs = _snap_reg.by_index(index);
					Genode::String<32> name { fs.snap_gen() };

					out = {
						.fileno = (Genode::addr_t)this | index,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { name.string() },
					};
					out_count = sizeof(Dirent);
					return READ_OK;
				} catch (Snapshot_registry::Invalid_index) {
					return READ_ERR_INVALID;
				}
			}

			Read_result _query_root(size_t  index,
			                        size_t &out_count,
			                        Dirent &out)
			{
				if (index == 0) {
					out = {
						.fileno = (Genode::addr_t)this,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { "snapshots" }
					};
				} else {
					out.type = Dirent_type::END;
				}

				out_count = sizeof(Dirent);
				return READ_OK;
			}

			Dir_vfs_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Snapshot_registry const &snap_reg,
			               bool root_dir)
			:
				Snap_vfs_handle(ds, fs, alloc, 0),
				_snap_reg(snap_reg), _root_dir(root_dir)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;

				if (dst.num_bytes < sizeof(Dirent))
					return READ_ERR_INVALID;

				size_t index = size_t(seek() / sizeof(Dirent));

				Dirent &out = *(Dirent*)dst.start;

				if (!_root_dir) {

					/* opended as "/snapshots" */
					return _query_snapshots(index, out_count, out);

				} else {
					/* opened as "/" */
					return _query_root(index, out_count, out);
				}
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				return WRITE_ERR_INVALID;
			}

			bool read_ready() const override { return true; }
		};

		struct Dir_snap_vfs_handle : Vfs::Vfs_handle
		{
			Vfs_handle &vfs_handle;

			Dir_snap_vfs_handle(Directory_service &ds,
			                    File_io_service   &fs,
			                    Genode::Allocator &alloc,
			                    Vfs::Vfs_handle   &vfs_handle)
			: Vfs_handle(ds, fs, alloc, 0), vfs_handle(vfs_handle) { }

			~Dir_snap_vfs_handle()
			{
				vfs_handle.close();
			}
		};

		Snapshot_registry  _snap_reg;
		Tresor_adapter           &_adapter;

		char const *_sub_path(char const *path) const
		{
			/* skip heading slash in path if present */
			if (path[0] == '/') {
				path++;
			}

			Genode::size_t const name_len = strlen(type_name());
			if (strcmp(path, type_name(), name_len) != 0) {
				return nullptr;
			}

			path += name_len;

			/*
			 * The first characters of the first path element are equal to
			 * the current directory name. Let's check if the length of the
			 * first path element matches the name length.
			 */
			if (*path != 0 && *path != '/') {
				return 0;
			}

			return path;
		}


		Snapshots_file_system(Vfs::Env         &vfs_env,
		                      Genode::Xml_node  /* node */,
		                      Tresor_adapter          &adapter)
		:
			_vfs_env(vfs_env), _snap_reg(vfs_env.alloc(), adapter, *this), _adapter(adapter)
		{
			_adapter.manage_snapshots_file_system(*this);
		}

		static char const *type_name() { return "snapshots"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Dataspace_capability dataspace(char const * /* path */) override
		{
			return Genode::Dataspace_capability();
		}

		void release(char const * /* path */, Dataspace_capability) override
		{
		}

		Open_result open(char const       *path,
		                 unsigned          mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator        &alloc) override
		{
			path = _sub_path(path);
			if (!path || path[0] != '/') {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				return fs.open(path, mode, out_handle, alloc);
			} catch (Snapshot_registry::Invalid_path) { }

			return OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(char const       *path,
		                       bool              create,
		                       Vfs::Vfs_handle **out_handle,
		                       Allocator        &alloc) override
		{
			if (create) {
				return OPENDIR_ERR_PERMISSION_DENIED;
			}

			bool const top = _top_dir(path);
			if (_root_dir(path) || top) {
				_snap_reg.update(_vfs_env);

				*out_handle = new (alloc) Dir_vfs_handle(*this, *this, alloc,
				                                         _snap_reg, top);
				return OPENDIR_OK;
			} else {
				char const *sub_path = _sub_path(path);
				if (!sub_path) {
					return OPENDIR_ERR_LOOKUP_FAILED;
				}
				try {
					Snapshot_file_system &fs = _snap_reg.by_path(sub_path);
					Vfs::Vfs_handle *handle = nullptr;
					Opendir_result const res = fs.opendir(sub_path, create, &handle, alloc);
					if (res != OPENDIR_OK) {
						return OPENDIR_ERR_LOOKUP_FAILED;
					}
					*out_handle = new (alloc) Dir_snap_vfs_handle(*this, *this,
					                                              alloc, *handle);
					return OPENDIR_OK;
				} catch (Snapshot_registry::Invalid_path) { }
			}
			return OPENDIR_ERR_LOOKUP_FAILED;
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Stat_result stat(char const *path, Stat &out_stat) override
		{
			out_stat = Stat { };
			path = _sub_path(path);

			/* path does not match directory name */
			if (!path) {
				return STAT_ERR_NO_ENTRY;
			}

			/*
			 * If path equals directory name, return information about the
			 * current directory.
			 */
			if (strlen(path) == 0 || _top_dir(path)) {

				out_stat.type   = Node_type::DIRECTORY;
				out_stat.inode  = 1;
				out_stat.device = (Genode::addr_t)this;
				return STAT_OK;
			}

			if (!path || path[0] != '/') {
				return STAT_ERR_NO_ENTRY;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				Stat_result const res = fs.stat(path, out_stat);
				return res;
			} catch (Snapshot_registry::Invalid_path) { }

			return STAT_ERR_NO_ENTRY;
		}

		Unlink_result unlink(char const * /* path */) override
		{
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const * /* from */, char const * /* to */) override
		{
			return RENAME_ERR_NO_PERM;
		}

		file_size num_dirent(char const *path) override
		{
			if (_top_dir(path)) {
				return 1;
			}
			if (_root_dir(path)) {
				_snap_reg.update(_vfs_env);
				file_size const num = _snap_reg.number_of_snapshots();
				return num;
			}
			_snap_reg.update(_vfs_env);

			path = _sub_path(path);
			if (!path) {
				return 0;
			}
			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				file_size const num = fs.num_dirent(path);
				return num;
			} catch (Snapshot_registry::Invalid_path) {
				return 0;
			}
		}

		bool directory(char const *path) override
		{
			if (_root_dir(path)) {
				return true;
			}

			path = _sub_path(path);
			if (!path) {
				return false;
			}
			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				return fs.directory(path);
			} catch (Snapshot_registry::Invalid_path) { }

			return false;
		}

		char const *leaf_path(char const *path) override
		{
			path = _sub_path(path);
			if (!path) {
				return nullptr;
			}

			if (strlen(path) == 0 || strcmp(path, "") == 0) {
				return path;
			}

			try {
				Snapshot_file_system &fs = _snap_reg.by_path(path);
				char const *leaf_path = fs.leaf_path(path);
				if (leaf_path) {
					return leaf_path;
				}
			} catch (Snapshot_registry::Invalid_path) { }

			return nullptr;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs::Vfs_handle * /* vfs_handle */,
		                   Const_byte_range_ptr const &, size_t & /* out_count */) override
		{
			return WRITE_ERR_IO;
		}

		bool queue_read(Vfs::Vfs_handle *vfs_handle, size_t size) override
		{
			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().queue_read(&dh->vfs_handle, size);
			}

			return true;
		}

		Read_result complete_read(Vfs::Vfs_handle *vfs_handle,
		                          Byte_range_ptr const &dst,
		                          size_t & out_count) override
		{
			Snap_vfs_handle *sh =
				dynamic_cast<Snap_vfs_handle*>(vfs_handle);
			if (sh) {
				Read_result const res = sh->read(dst, out_count);
				return res;
			}

			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().complete_read(&dh->vfs_handle,
				                                         dst, out_count);
			}

			return READ_ERR_IO;
		}

		bool read_ready(Vfs::Vfs_handle const &) const override {
			return true; }

		bool write_ready(Vfs::Vfs_handle const &) const override {
			return false; }

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs_tresor::Control_local_factory : File_system_factory
{
	Tresor_adapter                      &_adapter;
	Rekey_file_system             _rekeying_fs;
	Rekey_progress_file_system    _rekeying_progress_fs;
	Deinitialize_file_system      _deinitialize_fs;
	Create_snapshot_file_system   _create_snapshot_fs;
	Discard_snapshot_file_system  _discard_snapshot_fs;
	Extend_file_system            _extend_fs;
	Extend_progress_file_system   _extend_progress_fs;

	Control_local_factory(Vfs::Env & /* env */,
	                      Xml_node   /* config */,
	                      Tresor_adapter  & adapter)
	:
		_adapter(adapter),
		_rekeying_fs(adapter),
		_rekeying_progress_fs(adapter),
		_deinitialize_fs(adapter),
		_create_snapshot_fs(adapter),
		_discard_snapshot_fs(adapter),
		_extend_fs(adapter),
		_extend_progress_fs(adapter)
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
		if (node.has_type(Rekey_file_system::type_name())) {
			return &_rekeying_fs;
		}

		if (node.has_type(Rekey_progress_file_system::type_name())) {
			return &_rekeying_progress_fs;
		}

		if (node.has_type(Deinitialize_file_system::type_name())) {
			return &_deinitialize_fs;
		}

		if (node.has_type(Create_snapshot_file_system::type_name())) {
			return &_create_snapshot_fs;
		}

		if (node.has_type(Discard_snapshot_file_system::type_name())) {
			return &_discard_snapshot_fs;
		}

		if (node.has_type(Extend_file_system::type_name())) {
			return &_extend_fs;
		}

		if (node.has_type(Extend_progress_file_system::type_name())) {
			return &_extend_progress_fs;
		}

		return nullptr;
	}
};


class Vfs_tresor::Control_file_system : private Control_local_factory,
                                        public Vfs::Dir_file_system
{
	private:

		typedef String<256> Config;

		static Config _config(Xml_node /* node */)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "control");
				xml.node("rekey", [&] () { });
				xml.node("rekey_progress", [&] () { });
				xml.node("extend", [&] () { });
				xml.node("extend_progress", [&] () { });
				xml.node("create_snapshot", [&] () { });
				xml.node("discard_snapshot", [&] () { });
				xml.node("deinitialize", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		Control_file_system(Vfs::Env         &vfs_env,
		                    Genode::Xml_node  node,
		                    Tresor_adapter          &tresor)
		:
			Control_local_factory(vfs_env, node, tresor),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this)
		{ }

		static char const *type_name() { return "control"; }

		char const *type() override { return type_name(); }
};


struct Vfs_tresor::Local_factory : File_system_factory
{
	Tresor_adapter               &_adapter;
	Snapshot_file_system   _current_snapshot_fs;
	Snapshots_file_system  _snapshots_fs;
	Control_file_system    _control_fs;

	Local_factory(Vfs::Env &env, Xml_node config,
	              Tresor_adapter &adapter)
	:
		_adapter(adapter),
		_current_snapshot_fs(env, adapter, 0, false),
		_snapshots_fs(env, config, adapter),
		_control_fs(env, config, adapter)
	{ }

	~Local_factory()
	{
		_adapter.dissolve_snapshots_file_system(_snapshots_fs);
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		using Name = String<64>;
		if (node.has_type(Snapshot_file_system::type_name())
		    && node.attribute_value("name", Name()) == "current")
			return &_current_snapshot_fs;

		if (node.has_type(Control_file_system::type_name()))
			return &_control_fs;

		if (node.has_type(Snapshots_file_system::type_name()))
			return &_snapshots_fs;

		return nullptr;
	}
};


class Vfs_tresor::File_system : private Local_factory,
                                public Vfs::Dir_file_system
{
	private:

		Tresor_adapter &_adapter;

		typedef String<256> Config;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				typedef String<64> Name;

				xml.attribute("name",
				              node.attribute_value("name",
				                                   Name("tresor")));

				xml.node("control", [&] () { });

				xml.node("snapshot", [&] () {
					xml.attribute("name", "current");
				});

				xml.node("snapshots", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node,
		            Tresor_adapter &adapter)
		:
			Local_factory(vfs_env, node, adapter),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this),
			_adapter(adapter)
		{ }

		~File_system()
		{
			/*
			 * XXX rather then destroying the adapter here, it should be
			 *     done on the out-side where it was allocated in the first
			 *     place but the factory interface does not support that yet
			 *     destroy(vfs_env.alloc().alloc()), &_adapter);
			 */
		}
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			try {
				/* XXX adapter is not managed and will leak */
				Vfs_tresor::Tresor_adapter *adapter =
					new (vfs_env.alloc()) Vfs_tresor::Tresor_adapter { vfs_env, node };
				return new (vfs_env.alloc())
					Vfs_tresor::File_system(vfs_env, node, *adapter);
			} catch (...) {
				Genode::error("could not create 'tresor_fs' ");
			}
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}


/**********************
 ** Vfs_tresor::Tresor_adapter **
 **********************/

void Vfs_tresor::Tresor_adapter::_snapshots_fs_update_snapshot_registry()
{
	if (_snapshots_fs_ptr)
		_snapshots_fs_ptr->update_snapshot_registry();
}


void Vfs_tresor::Tresor_adapter::_extend_fs_trigger_watch_response()
{
	if (_extend_fs_ptr)
		_extend_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::_extend_progress_fs_trigger_watch_response()
{
	if (_extend_progress_fs_ptr)
		_extend_progress_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::_rekey_fs_trigger_watch_response()
{
	if (_rekey_fs_ptr)
		_rekey_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::_rekey_progress_fs_trigger_watch_response()
{
	if (_rekey_progress_fs_ptr)
		_rekey_progress_fs_ptr->trigger_watch_response();
}


void Vfs_tresor::Tresor_adapter::_deinit_fs_trigger_watch_response()
{
	if (_deinit_fs_ptr)
		_deinit_fs_ptr->trigger_watch_response();
}


/*******************************************************
 ** Vfs_tresor::Snapshots_file_system::Snapshot_registry **
 *******************************************************/

void Vfs_tresor::Snapshots_file_system::Snapshot_registry::update(Vfs::Env &vfs_env)
{
	Tresor::Snapshots_info snap_info { };
	_adapter.snapshots_info(snap_info);
	bool trigger_watch_response { false };

	/* alloc new */
	for (size_t i = 0; i < MAX_NR_OF_SNAPSHOTS; i++) {

		Generation const snap_gen = snap_info.generations[i];
		if (snap_gen == INVALID_GENERATION)
			continue;

		bool is_old = false;
		auto find_old = [&] (Snapshot_file_system const &fs) {
			is_old |= (fs.snap_gen() == snap_gen);
		};
		_registry.for_each(find_old);

		if (!is_old) {

			new (_alloc)
				Genode::Registered<Snapshot_file_system> {
					_registry, vfs_env, _adapter, snap_gen, true };

			++_number_of_snapshots;
			trigger_watch_response = true;
		}
	}

	/* destroy old */
	auto find_stale = [&] (Snapshot_file_system const &fs)
	{
		bool is_stale = true;
		for (size_t i = 0; i < MAX_NR_OF_SNAPSHOTS; i++) {
			Generation const snap_gen = snap_info.generations[i];
			if (snap_gen == INVALID_GENERATION)
				continue;

			if (fs.snap_gen() == snap_gen) {
				is_stale = false;
				break;
			}
		}

		if (is_stale) {
			destroy(&_alloc, &const_cast<Snapshot_file_system&>(fs));
			--_number_of_snapshots;
			trigger_watch_response = true;
		}
	};
	_registry.for_each(find_stale);
	if (trigger_watch_response) {
		_snapshots_fs.trigger_watch_response();
	}
}
