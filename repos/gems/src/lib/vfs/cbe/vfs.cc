/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
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

/* CBE tester includes */
#include <block_io.h>
#include <client_data.h>
#include <crypto.h>
#include <free_tree.h>
#include <meta_tree.h>
#include <request_pool.h>
#include <superblock_control.h>
#include <trust_anchor.h>
#include <virtual_block_device.h>


namespace Vfs_cbe {
	using namespace Vfs;
	using namespace Genode;
	using namespace Cbe;

	class Data_file_system;

	class Extend_file_system;
	class Rekey_file_system;
	class Deinitialize_file_system;
	class Create_snapshot_file_system;
	class Discard_snapshot_file_system;

	struct Control_local_factory;
	class  Control_file_system;

	struct Snapshot_local_factory;
	class  Snapshot_file_system;

	struct Snapshots_local_factory;
	class  Snapshots_file_system;

	struct Local_factory;
	class  File_system;

	class Wrapper;

	template <typename T>
	class Pointer
	{
		private:

			T *_obj;

		public:

			struct Invalid : Genode::Exception { };

			Pointer() : _obj(nullptr) { }

			Pointer(T &obj) : _obj(&obj) { }

			T &obj() const
			{
				if (_obj == nullptr)
					throw Invalid();

				return *_obj;
			}

			bool valid() const { return _obj != nullptr; }
	};
}


namespace Cbe {

	char const *module_name(unsigned long id)
	{
		switch (id) {
		case CRYPTO: return "crypto";
		case BLOCK_IO: return "block_io";
		case CBE_LIBRARA: return "cbe";
		case CBE_INIT_LIBRARA: return "cbe_init";
		case CACHE: return "cache";
		case META_TREE: return "meta_tree";
		case FREE_TREE: return "free_tree";
		case VIRTUAL_BLOCK_DEVICE: return "vbd";
		case SUPERBLOCK_CONTROL: return "sb_control";
		case CLIENT_DATA: return "client_data";
		case TRUST_ANCHOR: return "trust_anchor";
		case COMMAND_POOL: return "command_pool";
		case BLOCK_ALLOCATOR: return "block_allocator";
		case VBD_INITIALIZER: return "vbd_initializer";
		case FT_INITIALIZER: return "ft_initializer";
		case SB_INITIALIZER: return "sb_initializer";
		case REQUEST_POOL: return "request_pool";
		default: break;
		}
		return "?";
	}
}


extern "C" void adainit();


extern "C" void print_u8(unsigned char const u) { Genode::log(u); }


class Vfs_cbe::Wrapper : public Cbe::Module
{
	private:

		Vfs::Env &_vfs_env;

		Constructible<Request_pool>         _request_pool { };
		Constructible<Cbe::Free_tree>       _free_tree    { };
		Constructible<Virtual_block_device> _vbd          { };
		Constructible<Superblock_control>   _sb_control   { };
		Cbe::Meta_tree                      _meta_tree    { };
		Constructible<Cbe::Trust_anchor>    _trust_anchor { };
		Constructible<Cbe::Crypto>          _crypto       { };
		Constructible<Cbe::Block_io>        _block_io     { };

		Client_data_request _client_data_request { };

		Cbe::Module *_module_ptrs[MAX_MODULE_ID+1] { };

	public:

		/********************************
		 ** Module API for Client_data **
		 ********************************/

		bool ready_to_submit_request() override
		{
			return _client_data_request._type == Client_data_request::INVALID;
		}

		void submit_request(Module_request &req) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {

				class Exception_1 { };
				throw Exception_1 { };
			}
			req.dst_request_id(0);
			_client_data_request = *dynamic_cast<Client_data_request *>(&req);
			switch (_client_data_request._type) {
			case Client_data_request::OBTAIN_PLAINTEXT_BLK:
			{
				void const *src =
					_lookup_write_buffer(_client_data_request._client_req_tag,
					                     _client_data_request._vba);
				if (src == nullptr) {
					_client_data_request._success = false;
					break;
				}

				(void)memcpy((void*)_client_data_request._plaintext_blk_ptr,
				             src, sizeof(Cbe::Block_data));

				_client_data_request._success = true;
				break;
			}
			case Client_data_request::SUPPLY_PLAINTEXT_BLK:
			{
				void *dst =
					_lookup_read_buffer(_client_data_request._client_req_tag,
					                    _client_data_request._vba);
				if (dst == nullptr) {
					_client_data_request._success = false;
					break;
				}

				(void)memcpy(dst, (void const*)_client_data_request._plaintext_blk_ptr,
				             sizeof(Cbe::Block_data));

				_client_data_request._success = true;
				break;
			}
			case Client_data_request::INVALID:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void execute(bool &progress) override
		{
			if (_helper_read_request.pending()) {
				if (_request_pool->ready_to_submit_request()) {
					_helper_read_request.cbe_request.snap_id(
						_frontend_request.cbe_request.snap_id());
					_request_pool->submit_request(_helper_read_request.cbe_request);
					_helper_read_request.state = Helper_request::State::IN_PROGRESS;
				}
			}

			if (_helper_write_request.pending()) {
				if (_request_pool->ready_to_submit_request()) {
					_helper_write_request.cbe_request.snap_id(
						_frontend_request.cbe_request.snap_id());
					_request_pool->submit_request(_helper_write_request.cbe_request);
					_helper_write_request.state = Helper_request::State::IN_PROGRESS;
				}
			}

			if (_frontend_request.pending()) {

				using ST = Frontend_request::State;

				Cbe::Request &request = _frontend_request.cbe_request;

				if (_request_pool->ready_to_submit_request()) {
					_request_pool->submit_request(request);
					_frontend_request.state = ST::IN_PROGRESS;
					progress = true;
				}
			}
		}

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {
				if (sizeof(_client_data_request) > buf_size) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				Genode::memcpy(buf_ptr, &_client_data_request,
				               sizeof(_client_data_request));;
				return true;
			}
			return false;
		}

		void _drop_completed_request(Module_request &) override
		{
			if (_client_data_request._type == Client_data_request::INVALID) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_client_data_request._type = Client_data_request::INVALID;
		}

		struct Rekeying
		{
			enum State { UNKNOWN, IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			State    state;
			Result   last_result;
			uint32_t key_id;

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::UNKNOWN:     return "unknown";
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}

				return "-";
			}
		};

		struct Deinitialize
		{
			enum State { IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			State    state;
			Result   last_result;
			uint32_t key_id;

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}

				return "-";
			}
		};

		struct Extending
		{
			enum Type { INVALID, VBD, FT };
			enum State { UNKNOWN, IDLE, IN_PROGRESS, };
			enum Result { NONE, SUCCESS, FAILED, };
			Type   type;
			State  state;
			Result last_result;

			static char const *state_to_cstring(State const s)
			{
				switch (s) {
				case State::UNKNOWN:     return "unknown";
				case State::IDLE:        return "idle";
				case State::IN_PROGRESS: return "in-progress";
				}

				return "-";
			}

			static Type string_to_type(char const *s)
			{
				if (Genode::strcmp("vbd", s, 3) == 0) {
					return Type::VBD;
				} else

				if (Genode::strcmp("ft", s, 2) == 0) {
					return Type::FT;
				}

				return Type::INVALID;
			}
		};

	private:

		Rekeying _rekey_obj {
			.state       = Rekeying::State::UNKNOWN,
			.last_result = Rekeying::Result::NONE,
			.key_id      = 0, };

		Deinitialize _deinit_obj
		{
			.state       = Deinitialize::State::IDLE,
			.last_result = Deinitialize::Result::NONE
		};

		Extending _extend_obj {
			.type        = Extending::Type::INVALID,
			.state       = Extending::State::UNKNOWN,
			.last_result = Extending::Result::NONE,
		};

		Pointer<Snapshots_file_system>    _snapshots_fs { };
		Pointer<Extend_file_system>       _extend_fs    { };
		Pointer<Rekey_file_system>        _rekey_fs     { };
		Pointer<Deinitialize_file_system> _deinit_fs    { };

		/* configuration options */
		bool _verbose       { false };
		bool _debug         { false };

		void _read_config(Xml_node config)
		{
			_verbose      = true;
			_debug        = config.attribute_value("debug",   _debug);
		}

		struct Could_not_open_block_backend : Genode::Exception { };
		struct No_valid_superblock_found    : Genode::Exception { };

		void _initialize_cbe()
		{
			_free_tree.construct();
			_modules_add(FREE_TREE, *_free_tree);

			_vbd.construct();
			_modules_add(VIRTUAL_BLOCK_DEVICE, *_vbd);

			_sb_control.construct();
			_modules_add(SUPERBLOCK_CONTROL, *_sb_control);

			_request_pool.construct();
			_modules_add(REQUEST_POOL, *_request_pool);
		}

		/************************
		 ** Module composition **
		 ************************/

		enum { VERBOSE_MODULE_COMMUNICATION = 0 };

		void _modules_add(unsigned long  module_id,
		                  Module        &module)
		{
			if (module_id > MAX_MODULE_ID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] != nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = &module;
		}

		void _modules_remove(unsigned long  module_id)
		{
			if (module_id > MAX_MODULE_ID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] == nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = nullptr;
		}

		void _modules_execute(bool &progress)
		{
			for (unsigned long id { 0 }; id <= MAX_MODULE_ID; id++) {

				if (_module_ptrs[id] == nullptr)
					continue;

				Module *module_ptr { _module_ptrs[id] };
				module_ptr->execute(progress);
				module_ptr->for_each_generated_request([&] (Module_request &req) {
					if (req.dst_module_id() > MAX_MODULE_ID) {
						class Bad_dst_module { };
						throw Bad_dst_module { };
					}
					Module &dst_module { *_module_ptrs[req.dst_module_id()] };
					if (!dst_module.ready_to_submit_request()) {

						if (VERBOSE_MODULE_COMMUNICATION)
							Genode::log(
								module_name(id), ":", req.src_request_id_str(),
								" --", req.type_name(), "-| ",
								module_name(req.dst_module_id()));

						return Module::REQUEST_NOT_HANDLED;
					}
					dst_module.submit_request(req);

					if (VERBOSE_MODULE_COMMUNICATION)
						Genode::log(
							module_name(id), ":", req.src_request_id_str(),
							" --", req.type_name(), "--> ",
							module_name(req.dst_module_id()), ":",
							req.dst_request_id_str());

					progress = true;
					return Module::REQUEST_HANDLED;
				});
				module_ptr->for_each_completed_request([&] (Module_request &req) {
					if (req.src_module_id() > MAX_MODULE_ID) {
						class Bad_src_module { };
						throw Bad_src_module { };
					}
					if (VERBOSE_MODULE_COMMUNICATION)
						Genode::log(
							module_name(req.src_module_id()), ":",
							req.src_request_id_str(), " <--", req.type_name(),
							"-- ", module_name(id), ":",
							req.dst_request_id_str());
					Module &src_module { *_module_ptrs[req.src_module_id()] };
					src_module.generated_request_complete(req);
					progress = true;
				});
			}
		}

		/*****************************
		 ** COMMAND_POOL Module API **
		 *****************************/

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			return false;
		}

		void _drop_generated_request(Module_request &mod_req) override { }

	public:

		void generated_request_complete(Module_request &mod_req) override
		{
			using ST = Frontend_request::State;

			switch (mod_req.dst_module_id()) {
			case REQUEST_POOL:
			{
				Request const &cbe_request {
					*static_cast<Request *>(&mod_req)};

				if (cbe_request.operation() == Cbe::Request::Operation::REKEY) {
					bool const req_sucess = cbe_request.success();
					if (_verbose) {
						log("Complete request: backend request (", cbe_request, ")");
					}
					_rekey_obj.state = Rekeying::State::IDLE;
					_rekey_obj.last_result = req_sucess ? Rekeying::Result::SUCCESS
					                                    : Rekeying::Result::FAILED;

					_rekey_fs_trigger_watch_response();
					break;
				}

				if (cbe_request.operation() == Cbe::Request::Operation::DEINITIALIZE) {
					bool const req_sucess = cbe_request.success();
					if (_verbose) {
						log("Complete request: backend request (", cbe_request, ")");
					}
					_deinit_obj.state = Deinitialize::State::IDLE;
					_deinit_obj.last_result = req_sucess ? Deinitialize::Result::SUCCESS
					                                     : Deinitialize::Result::FAILED;

					_deinit_fs_trigger_watch_response();
					break;
				}

				if (cbe_request.operation() == Cbe::Request::Operation::EXTEND_VBD) {
					bool const req_sucess = cbe_request.success();
					if (_verbose) {
						log("Complete request: backend request (", cbe_request, ")");
					}
					_extend_obj.state = Extending::State::IDLE;
					_extend_obj.last_result =
						req_sucess ? Extending::Result::SUCCESS
						           : Extending::Result::FAILED;

					_extend_fs_trigger_watch_response();
					break;
				}

				if (cbe_request.operation() == Cbe::Request::Operation::EXTEND_FT) {
					bool const req_sucess = cbe_request.success();
					if (_verbose) {
						log("Complete request: backend request (", cbe_request, ")");
					}
					_extend_obj.state = Extending::State::IDLE;
					_extend_obj.last_result =
						req_sucess ? Extending::Result::SUCCESS
						           : Extending::Result::FAILED;

					_extend_fs_trigger_watch_response();
					break;
				}

				if (cbe_request.operation() == Cbe::Request::Operation::CREATE_SNAPSHOT) {
					if (_verbose) {
						log("Complete request: (", cbe_request, ")");
					}
					_create_snapshot_request.cbe_request = Cbe::Request();
					_snapshots_fs_update_snapshot_registry();
					break;
				}

				if (cbe_request.operation() == Cbe::Request::Operation::DISCARD_SNAPSHOT) {
					if (_verbose) {
						log("Complete request: (", cbe_request, ")");
					}
					_discard_snapshot_request.cbe_request = Cbe::Request();
					_snapshots_fs_update_snapshot_registry();
					break;
				}

				if (!cbe_request.success()) {
					_helper_read_request.state  = Helper_request::State::NONE;
					_helper_write_request.state = Helper_request::State::NONE;

					bool const eof = cbe_request.block_number() > _sb_control->max_vba();
					_frontend_request.state = eof ? ST::ERROR_EOF : ST::ERROR;
					_frontend_request.cbe_request.success(false);
					if (_verbose) {
						Genode::log("Request failed: ",
						            " (frontend request: ", _frontend_request.cbe_request,
						            " count: ", _frontend_request.count, ")");
					}
					break;
				}

				if (_helper_read_request.in_progress()) {
					_helper_read_request.state = Helper_request::State::COMPLETE;
					_helper_read_request.cbe_request.success(
						cbe_request.success());
				} else if (_helper_write_request.in_progress()) {
					_helper_write_request.state = Helper_request::State::COMPLETE;
					_helper_write_request.cbe_request.success(
						cbe_request.success());
				} else {
					_frontend_request.state = ST::COMPLETE;
					_frontend_request.cbe_request.success(cbe_request.success());
					if (_verbose) {
						Genode::log("Complete request: ",
						            " (frontend request: ", _frontend_request.cbe_request,
						            " count: ", _frontend_request.count, ")");
					}
				}

				if (_helper_read_request.complete()) {
					if (_frontend_request.cbe_request.read()) {
						char       * dst = reinterpret_cast<char*>
							(_frontend_request.cbe_request.offset());
						char const * src = reinterpret_cast<char const*>
							(&_helper_read_request.block_data) + _frontend_request.helper_offset;

						Genode::memcpy(dst, src, _frontend_request.count);

						_helper_read_request.state = Helper_request::State::NONE;
						_frontend_request.state = ST::COMPLETE;
						_frontend_request.cbe_request.success(
							_helper_read_request.cbe_request.success());

						if (_verbose) {
							Genode::log("Complete unaligned READ request: ",
										" (frontend request: ", _frontend_request.cbe_request,
										" (helper request: ", _helper_read_request.cbe_request,
										" offset: ", _frontend_request.helper_offset,
										" count: ", _frontend_request.count, ")");
						}
					}

					if (_frontend_request.cbe_request.write()) {
						/* copy whole block first */
						{
							char       * dst = reinterpret_cast<char*>
								(&_helper_write_request.block_data);
							char const * src = reinterpret_cast<char const*>
								(&_helper_read_request.block_data);
							Genode::memcpy(dst, src, sizeof (Cbe::Block_data));
						}

						/* and than actual request data */
						{
							char       * dst = reinterpret_cast<char*>
								(&_helper_write_request.block_data) + _frontend_request.helper_offset;
							char const * src = reinterpret_cast<char const*>
								(_frontend_request.cbe_request.offset());
							Genode::memcpy(dst, src, _frontend_request.count);
						}

						/* re-use request */
						_helper_write_request.cbe_request = Cbe::Request(
							Cbe::Request::Operation::WRITE,
							false,
							_helper_read_request.cbe_request.block_number(),
							(uint64_t) &_helper_write_request.block_data,
							_helper_read_request.cbe_request.count(),
							_helper_read_request.cbe_request.key_id(),
							_helper_read_request.cbe_request.tag(),
							_helper_read_request.cbe_request.snap_id(),
							COMMAND_POOL, 0);

						_helper_write_request.state = Helper_request::State::PENDING;
						_helper_read_request.state  = Helper_request::State::NONE;
					}
				}

				if (_helper_write_request.complete()) {
					if (_verbose) {
						Genode::log("Complete unaligned WRITE request: ",
									" (frontend request: ", _frontend_request.cbe_request,
									" (helper request: ", _helper_read_request.cbe_request,
									" offset: ", _frontend_request.helper_offset,
									" count: ", _frontend_request.count, ")");
					}

					_helper_write_request.state = Helper_request::State::NONE;
					_frontend_request.state = ST::COMPLETE;
				}
				break;
			}
			default:
				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void manage_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			if (_snapshots_fs.valid()) {

				class Already_managing_an_snapshots_file_system { };
				throw Already_managing_an_snapshots_file_system { };
			}
			_snapshots_fs = snapshots_fs;
		}

		void dissolve_snapshots_file_system(Snapshots_file_system &snapshots_fs)
		{
			if (_snapshots_fs.valid()) {

				if (&_snapshots_fs.obj() != &snapshots_fs) {

					class Snapshots_file_system_not_managed { };
					throw Snapshots_file_system_not_managed { };
				}
				_snapshots_fs = Pointer<Snapshots_file_system> { };

			} else {

				class No_snapshots_file_system_managed { };
				throw No_snapshots_file_system_managed { };
			}
		}

		void manage_extend_file_system(Extend_file_system &extend_fs)
		{
			if (_extend_fs.valid()) {

				class Already_managing_an_extend_file_system { };
				throw Already_managing_an_extend_file_system { };
			}
			_extend_fs = extend_fs;
		}

		void dissolve_extend_file_system(Extend_file_system &extend_fs)
		{
			if (_extend_fs.valid()) {

				if (&_extend_fs.obj() != &extend_fs) {

					class Extend_file_system_not_managed { };
					throw Extend_file_system_not_managed { };
				}
				_extend_fs = Pointer<Extend_file_system> { };

			} else {

				class No_extend_file_system_managed { };
				throw No_extend_file_system_managed { };
			}
		}

		void manage_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			if (_rekey_fs.valid()) {

				class Already_managing_an_rekey_file_system { };
				throw Already_managing_an_rekey_file_system { };
			}
			_rekey_fs = rekey_fs;
		}

		void dissolve_rekey_file_system(Rekey_file_system &rekey_fs)
		{
			if (_rekey_fs.valid()) {

				if (&_rekey_fs.obj() != &rekey_fs) {

					class Rekey_file_system_not_managed { };
					throw Rekey_file_system_not_managed { };
				}
				_rekey_fs = Pointer<Rekey_file_system> { };

			} else {

				class No_rekey_file_system_managed { };
				throw No_rekey_file_system_managed { };
			}
		}

		void manage_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			if (_deinit_fs.valid()) {

				class Already_managing_an_deinit_file_system { };
				throw Already_managing_an_deinit_file_system { };
			}
			_deinit_fs = deinit_fs;
		}

		void dissolve_deinit_file_system(Deinitialize_file_system &deinit_fs)
		{
			if (_deinit_fs.valid()) {

				if (&_deinit_fs.obj() != &deinit_fs) {

					class Deinitialize_file_system_not_managed { };
					throw Deinitialize_file_system_not_managed { };
				}
				_deinit_fs = Pointer<Deinitialize_file_system> { };

			} else {

				class No_deinit_file_system_managed { };
				throw No_deinit_file_system_managed { };
			}
		}

		template <typename FN>
		void with_node(char const *name, char const *path, FN const &fn)
		{
			char xml_buffer[128] { };

			Genode::Xml_generator xml {
				xml_buffer, sizeof(xml_buffer), name,
				[&] { xml.attribute("path", path); }
			};

			Genode::Xml_node node { xml_buffer, sizeof(xml_buffer) };
			fn(node);
		}

		Wrapper(Vfs::Env &vfs_env, Xml_node config) : _vfs_env { vfs_env }
		{
			_read_config(config);

			using S = Genode::String<32>;

			S const block_path =
				config.attribute_value("block", S());
			if (block_path.valid())
				with_node("block_io", block_path.string(),
					[&] (Xml_node const &node) {
						_block_io.construct(vfs_env, node);
					});

			S const trust_anchor_path =
				config.attribute_value("trust_anchor", S());
			if (trust_anchor_path.valid())
				with_node("trust_anchor", trust_anchor_path.string(),
					[&] (Xml_node const &node) {
						_trust_anchor.construct(vfs_env, node);
					});

			S const crypto_path =
				config.attribute_value("crypto", S());
			if (crypto_path.valid())
				with_node("crypto", crypto_path.string(),
					[&] (Xml_node const &node) {
						_crypto.construct(vfs_env, node);
					});

			_modules_add(COMMAND_POOL,  *this);
			_modules_add(META_TREE,     _meta_tree);
			_modules_add(CRYPTO,        *_crypto);
			_modules_add(TRUST_ANCHOR,  *_trust_anchor);
			_modules_add(CLIENT_DATA,  *this);
			_modules_add(BLOCK_IO,      *_block_io);

			_initialize_cbe();
		}

		Cbe::Request_pool &cbe()
		{
			if (!_request_pool.constructed()) {
				struct Cbe_Not_Initialized { };
				throw Cbe_Not_Initialized();
			}

			return *_request_pool;
		}

		Genode::uint64_t max_vba()
		{
			return _sb_control->max_vba();
		}

		struct Invalid_Request : Genode::Exception { };

		struct Helper_request
		{
			enum { BLOCK_SIZE = 512, };
			enum State { NONE, PENDING, IN_PROGRESS, COMPLETE, ERROR };

			State state { NONE };

			Cbe::Block_data block_data  { };
			Cbe::Request    cbe_request { };

			bool pending()     const { return state == PENDING; }
			bool in_progress() const { return state == IN_PROGRESS; }
			bool complete()    const { return state == COMPLETE; }
		};

		Helper_request _helper_read_request  { };
		Helper_request _helper_write_request { };

		struct Frontend_request
		{
			enum State {
				NONE,
				PENDING, IN_PROGRESS, COMPLETE,
				ERROR, ERROR_EOF
			};
			State        state       { NONE };
			file_size    count       { 0 };
			Cbe::Request cbe_request { };

			void *data { nullptr };

			uint64_t offset { 0 };
			uint64_t helper_offset { 0 };

			bool pending()     const { return state == PENDING; }
			bool in_progress() const { return state == IN_PROGRESS; }
			bool complete()    const { return state == COMPLETE; }

			static char const *state_to_string(State s)
			{
				switch (s) {
				case State::NONE:         return "NONE";
				case State::PENDING:      return "PENDING";
				case State::IN_PROGRESS:  return "IN_PROGRESS";
				case State::COMPLETE:     return "COMPLETE";
				case State::ERROR:        return "ERROR";
				case State::ERROR_EOF:    return "ERROR_EOF";
				}
				return "<unknown>";
			}
		};

		uint64_t _next_client_request_tag()
		{
			static uint64_t _client_request_tag { 0 };
			return _client_request_tag++;
		}

		void const *_lookup_write_buffer(Genode::uint64_t tag, Genode::uint64_t vba)
		{
			if (_helper_write_request.in_progress())
				return (void const*)&_helper_write_request.block_data;
			if (_frontend_request.in_progress())
				return (void const*)_frontend_request.data;

			return nullptr;
		}

		void *_lookup_read_buffer(Genode::uint64_t tag, Genode::uint64_t vba)
		{
			if (_helper_read_request.in_progress())
				return (void *)&_helper_read_request.block_data;
			if (_frontend_request.in_progress())
				return (void *)_frontend_request.data;

			return nullptr;
		}

		Frontend_request _frontend_request { };

		Frontend_request const & frontend_request() const
		{
			return _frontend_request;
		}

		void ack_frontend_request(Vfs_handle &handle)
		{
			// assert current state was *_COMPLETE
			_frontend_request.state = Frontend_request::State::NONE;
			_frontend_request.cbe_request = Cbe::Request { };
		}

		bool submit_frontend_request(Vfs_handle              &handle,
		                             char                    *data,
		                             file_size                count,
		                             Cbe::Request::Operation  op,
		                             uint32_t                 snap_id)
		{
			if (_frontend_request.state != Frontend_request::State::NONE) {
				return false;
			}

			uint64_t const tag = _next_client_request_tag();

			/* short-cut for SYNC requests */
			if (op == Cbe::Request::Operation::SYNC) {
				_frontend_request.cbe_request = Cbe::Request(
					op,
					false,
					0,
					0,
					1,
					0,
					tag,
					0,
					COMMAND_POOL, 0);
				_frontend_request.count   = 0;
				_frontend_request.state   = Frontend_request::State::PENDING;
				if (_verbose) {
					Genode::log("Req: (front req: ",
					            _frontend_request.cbe_request, ")");
				}
				return true;
			}

			file_size const offset = handle.seek();
			bool unaligned_request = false;

			/* unaligned request if any condition is true */
			unaligned_request |= (offset % Cbe::BLOCK_SIZE) != 0;
			unaligned_request |= (count < Cbe::BLOCK_SIZE);

			if ((count % Cbe::BLOCK_SIZE) != 0 &&
			    !unaligned_request)
			{
				count = count - (count % Cbe::BLOCK_SIZE);
			}

			if (unaligned_request) {
				_helper_read_request.cbe_request = Cbe::Request(
					Cbe::Request::Operation::READ,
					false,
					offset / Cbe::BLOCK_SIZE,
					(uint64_t)&_helper_read_request.block_data,
					1,
					0,
					tag,
					0,
					COMMAND_POOL, 0);
				_helper_read_request.state = Helper_request::State::PENDING;

				_frontend_request.helper_offset = (offset % Cbe::BLOCK_SIZE);
				if (count >= (Cbe::BLOCK_SIZE - _frontend_request.helper_offset)) {
					_frontend_request.count = Cbe::BLOCK_SIZE - _frontend_request.helper_offset;
				} else {
					_frontend_request.count = count;
				}

				/* skip handling by the CBE, helper requests will do that for us */
				_frontend_request.state = Frontend_request::State::IN_PROGRESS;

			} else {
				_frontend_request.count = count;
				_frontend_request.state = Frontend_request::State::PENDING;
			}

			_frontend_request.data   = data;
			_frontend_request.offset = offset;
			_frontend_request.cbe_request = Cbe::Request(
				op,
				false,
				offset / Cbe::BLOCK_SIZE,
				(uint64_t)data,
				(uint32_t)(count / Cbe::BLOCK_SIZE),
				0,
				tag,
				snap_id,
				COMMAND_POOL, 0);

			if (_verbose) {
				if (unaligned_request) {
					Genode::log("Unaligned req: ",
					            "off: ", offset, " bytes: ", count,
					            " (front req: ", _frontend_request.cbe_request,
					            " (helper req: ", _helper_read_request.cbe_request,
					            " off: ", _frontend_request.helper_offset,
					            " count: ", _frontend_request.count, ")");
				} else {
					Genode::log("Req: ",
					            "off: ", offset, " bytes: ", count,
					            " (front req: ", _frontend_request.cbe_request, ")");
				}
			}

			return true;
		}

		void _snapshots_fs_update_snapshot_registry();

		void _extend_fs_trigger_watch_response();

		void _rekey_fs_trigger_watch_response();

		void _deinit_fs_trigger_watch_response();

		void handle_frontend_request()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				_modules_execute(progress);
			}
			_vfs_env.io().commit();

			Cbe::Info const info = _sb_control->info();

			using ES = Extending::State;
			if (_extend_obj.state == ES::UNKNOWN && info.valid) {
				if (info.extending_ft) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::FT;
					_extend_fs_trigger_watch_response();

				} else

				if (info.extending_vbd) {

					_extend_obj.state = ES::IN_PROGRESS;
					_extend_obj.type  = Extending::Type::VBD;
					_extend_fs_trigger_watch_response();

				} else {

					_extend_obj.state = ES::IDLE;
					_extend_fs_trigger_watch_response();
				}
			}
			using RS = Rekeying::State;
			if (_rekey_obj.state == RS::UNKNOWN && info.valid) {
				_rekey_obj.state =
					info.rekeying ? RS::IN_PROGRESS : RS::IDLE;

				_rekey_fs_trigger_watch_response();
			}
		}

		bool client_request_acceptable()
		{
			return _request_pool->ready_to_submit_request();
		}

		bool start_rekeying()
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Cbe::Request req(
				Cbe::Request::Operation::REKEY,
				false,
				0, 0, 0,
				_rekey_obj.key_id,
				0, 0,
				COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_rekey_obj.state       = Rekeying::State::IN_PROGRESS;
			_rekey_obj.last_result = Rekeying::Rekeying::FAILED;
			_rekey_fs_trigger_watch_response();

			// XXX kick-off rekeying
			handle_frontend_request();
			return true;
		}

		Rekeying const rekeying_progress() const
		{
			return _rekey_obj;
		}

		bool start_deinitialize()
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Cbe::Request req(
				Cbe::Request::Operation::DEINITIALIZE,
				false,
				0, 0, 0,
				0,
				0, 0,
				COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_deinit_obj.state       = Deinitialize::State::IN_PROGRESS;
			_deinit_obj.last_result = Deinitialize::Deinitialize::FAILED;
			_deinit_fs_trigger_watch_response();

			// XXX kick-off deinitialize
			handle_frontend_request();
			return true;
		}

		Deinitialize const deinitialize_progress() const
		{
			return _deinit_obj;
		}


		bool start_extending(Extending::Type           type,
		                     Cbe::Number_of_blocks_new blocks)
		{
			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			Cbe::Request::Operation op =
				Cbe::Request::Operation::INVALID;

			switch (type) {
			case Extending::Type::VBD:
				op = Cbe::Request::Operation::EXTEND_VBD;
				break;
			case Extending::Type::FT:
				op = Cbe::Request::Operation::EXTEND_FT;
				break;
			case Extending::Type::INVALID:
				return false;
			}

			Cbe::Request req(op, false,
			                 0, 0, blocks, 0, 0, 0,
			                 COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (background req: ", req, ")");
			}

			_request_pool->submit_request(req);
			_extend_obj.type        = type;
			_extend_obj.state       = Extending::State::IN_PROGRESS;
			_extend_obj.last_result = Extending::Result::NONE;
			_extend_fs_trigger_watch_response();

			// XXX kick-off extending
			handle_frontend_request();
			return true;
		}

		Extending const extending_progress() const
		{
			return _extend_obj;
		}

		void active_snapshot_ids(Cbe::Active_snapshot_ids &ids)
		{
			if (!_request_pool.constructed()) {
				_initialize_cbe();
			}
			_sb_control->active_snapshot_ids(ids);
			handle_frontend_request();
		}


		Frontend_request _create_snapshot_request { };

		bool create_snapshot()
		{
			if (!_request_pool.constructed()) {
				_initialize_cbe();
			}

			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			if (_create_snapshot_request.cbe_request.valid()) {
				return false;
			}

			Cbe::Request::Operation const op =
				Cbe::Request::Operation::CREATE_SNAPSHOT;

			_create_snapshot_request.cbe_request =
				Cbe::Request(op, false, 0, 0, 1, 0, 0, 0,
				             COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (req: ", _create_snapshot_request.cbe_request, ")");
			}

			_request_pool->submit_request(_create_snapshot_request.cbe_request);

			_create_snapshot_request.state =
				Frontend_request::State::IN_PROGRESS;

			// XXX kick-off snapshot creation request
			handle_frontend_request();
			return true;
		}

		Frontend_request _discard_snapshot_request { };

		bool discard_snapshot(Cbe::Generation id)
		{
			if (!_request_pool.constructed()) {
				_initialize_cbe();
			}

			if (!_request_pool->ready_to_submit_request()) {
				return false;
			}

			if (_discard_snapshot_request.cbe_request.valid()) {
				return false;
			}

			Cbe::Request::Operation const op =
				Cbe::Request::Operation::DISCARD_SNAPSHOT;

			_discard_snapshot_request.cbe_request =
				Cbe::Request(op, false, 0, 0, 1, 0, 0, (uint32_t)id,
				             COMMAND_POOL, 0);

			if (_verbose) {
				Genode::log("Req: (req: ", _discard_snapshot_request.cbe_request, ")");
			}

			_request_pool->submit_request(_discard_snapshot_request.cbe_request);

			_discard_snapshot_request.state =
				Frontend_request::State::IN_PROGRESS;

			// XXX kick-off snapshot creation request
			handle_frontend_request();
			return true;
		}

		Genode::Mutex _frontend_mtx { };

		Genode::Mutex &frontend_mtx() { return _frontend_mtx; }
};


class Vfs_cbe::Data_file_system : public Single_file_system
{
	private:

		Wrapper &_w;
		uint32_t _snap_id;

	public:

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;
			uint32_t _snap_id { 0 };

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper       &w,
			           uint32_t snap_id)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w), _snap_id(snap_id)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable()) {
						return READ_QUEUED;
					}
					using Op = Cbe::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(*this, dst, count,
						                           Op::READ, _snap_id);
					if (!accepted) { return READ_ERR_IO; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return READ_QUEUED;
				}

				if (state == State::COMPLETE) {
					out_count = _w.frontend_request().count;
					_w.ack_frontend_request(*this);
					return READ_OK;
				}

				if (state == State::ERROR_EOF) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return READ_OK;
				}

				if (state == State::ERROR) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return READ_ERR_IO;
				}

				return READ_ERR_IO;
			}

			Write_result write(char const *src, file_size count,
			                   file_size &out_count) override
			{
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable())
						return Write_result::WRITE_ERR_WOULD_BLOCK;

					using Op = Cbe::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(*this, const_cast<char*>(src),
						                           count, Op::WRITE, _snap_id);
					if (!accepted) { return WRITE_ERR_IO; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return WRITE_ERR_WOULD_BLOCK;
				}

				if (state == State::COMPLETE) {
					out_count = _w.frontend_request().count;
					_w.ack_frontend_request(*this);
					return WRITE_OK;
				}

				if (state == State::ERROR_EOF) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return WRITE_OK;
				}

				if (state == State::ERROR) {
					out_count = 0;
					_w.ack_frontend_request(*this);
					return WRITE_ERR_IO;
				}

				return WRITE_ERR_IO;
			}

			Sync_result sync() override
			{
				Genode::Mutex::Guard guard { _w.frontend_mtx() };

				using State = Wrapper::Frontend_request::State;

				State state = _w.frontend_request().state;
				if (state == State::NONE) {

					if (!_w.client_request_acceptable()) {
						return SYNC_QUEUED;
					}
					using Op = Cbe::Request::Operation;

					bool const accepted =
						_w.submit_frontend_request(*this, nullptr, 0, Op::SYNC, 0);
					if (!accepted) { return SYNC_ERR_INVALID; }
				}

				_w.handle_frontend_request();
				state = _w.frontend_request().state;

				if (   state == State::PENDING
				    || state == State::IN_PROGRESS) {
					return SYNC_QUEUED;
				}

				if (state == State::COMPLETE) {
					_w.ack_frontend_request(*this);
					return SYNC_OK;
				}

				if (state == State::ERROR) {
					_w.ack_frontend_request(*this);
					return SYNC_ERR_INVALID;
				}

				return SYNC_ERR_INVALID;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

		Data_file_system(Wrapper &w, uint32_t snap_id)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<data/>")),
			_w(w), _snap_id(snap_id)
		{ }

		~Data_file_system() { /* XXX sync on close */ }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			try {
				(void)_w.cbe();
			} catch (...) {
				return STAT_ERR_NO_ENTRY;
			}

			Stat_result result = Single_file_system::stat(path, out);

			/* max_vba range is from 0 ... N - 1 */
			out.size = (_w.max_vba() + 1) * Cbe::BLOCK_SIZE;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}


		/***************************
		 ** File-system interface **
		 ***************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				(void)_w.cbe();
			} catch (...) {
				return OPEN_ERR_UNACCESSIBLE;
			}

			*out_handle =
				new (alloc) Vfs_handle(*this, *this, alloc, _w, _snap_id);

			return OPEN_OK;
		}

		static char const *type_name() { return "data"; }
		char const *type() override { return type_name(); }
};


class Vfs_cbe::Extend_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static Content_string content_string(Wrapper const &wrapper)
		{
			Wrapper::Extending const & extending_progress {
				wrapper.extending_progress() };

			bool const in_progress {
				extending_progress.state ==
					Wrapper::Extending::State::IN_PROGRESS };

			bool const last_result {
				!in_progress &&
				extending_progress.last_result !=
					Wrapper::Extending::Result::NONE };

			bool const success {
				extending_progress.last_result ==
					Wrapper::Extending::Result::SUCCESS };

			Content_string const result {
				Wrapper::Extending::state_to_cstring(extending_progress.state),
				" last-result:",
				last_result ? success ? "success" : "failed" : "none",
				"\n" };

			return result;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}
				Content_string const result { content_string(_w) };
				copy_cstring(dst, result.string(), count);
				size_t const length_without_nul = result.length() - 1;
				out_count = count > length_without_nul - 1 ?
				            length_without_nul : count;

				return READ_OK;
			}

			Write_result write(char const *src, file_size count, file_size &out_count) override
			{
				using Type = Wrapper::Extending::Type;
				using State = Wrapper::Extending::State;
				if (_w.extending_progress().state != State::IDLE) {
					return WRITE_ERR_IO;
				}

				char tree[16];
				Arg_string::find_arg(src, "tree").string(tree, sizeof (tree), "-");
				Type type = Wrapper::Extending::string_to_type(tree);
				if (type == Type::INVALID) {
					return WRITE_ERR_IO;
				}

				unsigned long blocks = Arg_string::find_arg(src, "blocks").ulong_value(0);
				if (blocks == 0) {
					return WRITE_ERR_IO;
				}

				bool const okay = _w.start_extending(type, blocks);
				if (!okay) {
					return WRITE_ERR_IO;
				}

				out_count = count;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Extend_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<extend/>")),
			_w(w)
		{
			_w.manage_extend_file_system(*this);
		}

		~Extend_file_system()
		{
			_w.dissolve_extend_file_system(*this);
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
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = content_string(_w).length() - 1;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_cbe::Rekey_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static Content_string content_string(Wrapper const &wrapper)
		{
			Wrapper::Rekeying const & rekeying_progress {
				wrapper.rekeying_progress() };

			bool const in_progress {
				rekeying_progress.state ==
					Wrapper::Rekeying::State::IN_PROGRESS };

			bool const last_result {
				!in_progress &&
				rekeying_progress.last_result !=
					Wrapper::Rekeying::Result::NONE };

			bool const success {
				rekeying_progress.last_result ==
					Wrapper::Rekeying::Result::SUCCESS };

			Content_string const result {
				Wrapper::Rekeying::state_to_cstring(rekeying_progress.state),
				" last-result:",
				last_result ? success ? "success" : "failed" : "none",
				"\n" };

			return result;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}
				Content_string const result { content_string(_w) };
				copy_cstring(dst, result.string(), count);
				size_t const length_without_nul = result.length() - 1;
				out_count = count > length_without_nul - 1 ?
				            length_without_nul : count;

				return READ_OK;
			}

			Write_result write(char const *src, file_size count, file_size &out_count) override
			{
				using State = Wrapper::Rekeying::State;
				if (_w.rekeying_progress().state != State::IDLE) {
					return WRITE_ERR_IO;
				}

				bool start_rekeying { false };
				Genode::ascii_to(src, start_rekeying);

				if (!start_rekeying) {
					return WRITE_ERR_IO;
				}

				if (!_w.start_rekeying()) {
					return WRITE_ERR_IO;
				}

				out_count = count;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Rekey_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<rekey/>")),
			_w(w)
		{
			_w.manage_rekey_file_system(*this);
		}

		~Rekey_file_system()
		{
			_w.dissolve_rekey_file_system(*this);
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
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = content_string(_w).length() - 1;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_cbe::Deinitialize_file_system : public Vfs::Single_file_system
{
	private:

		typedef Registered<Vfs_watch_handle>      Registered_watch_handle;
		typedef Registry<Registered_watch_handle> Watch_handle_registry;

		Watch_handle_registry _handle_registry { };

		Wrapper &_w;

		using Content_string = String<32>;

		static Content_string content_string(Wrapper const &wrapper)
		{
			Wrapper::Deinitialize const & deinitialize_progress {
				wrapper.deinitialize_progress() };

			bool const in_progress {
				deinitialize_progress.state ==
					Wrapper::Deinitialize::State::IN_PROGRESS };

			bool const last_result {
				!in_progress &&
				deinitialize_progress.last_result !=
					Wrapper::Deinitialize::Result::NONE };

			bool const success {
				deinitialize_progress.last_result ==
					Wrapper::Deinitialize::Result::SUCCESS };

			Content_string const result {
				Wrapper::Deinitialize::state_to_cstring(deinitialize_progress.state),
				" last-result:",
				last_result ? success ? "success" : "failed" : "none",
				"\n" };

			return result;
		}

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				if (seek() != 0) {
					out_count = 0;
					return READ_OK;
				}
				Content_string const result { content_string(_w) };
				copy_cstring(dst, result.string(), count);
				size_t const length_without_nul = result.length() - 1;
				out_count = count > length_without_nul - 1 ?
				            length_without_nul : count;

				return READ_OK;
			}

			Write_result write(char const *src, file_size count, file_size &out_count) override
			{
				using State = Wrapper::Deinitialize::State;
				if (_w.deinitialize_progress().state != State::IDLE) {
					return WRITE_ERR_IO;
				}

				bool start_deinitialize { false };
				Genode::ascii_to(src, start_deinitialize);

				if (!start_deinitialize) {
					return WRITE_ERR_IO;
				}

				if (!_w.start_deinitialize()) {
					return WRITE_ERR_IO;
				}

				out_count = count;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Deinitialize_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::rw(), Xml_node("<deinitialize/>")),
			_w(w)
		{
			_w.manage_deinit_file_system(*this);
		}

		~Deinitialize_file_system()
		{
			_w.dissolve_deinit_file_system(*this);
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
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = content_string(_w).length() - 1;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_cbe::Create_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Wrapper &_w;

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				return READ_ERR_IO;
			}

			Write_result write(char const *src, file_size count, file_size &out_count) override
			{
				bool create_snapshot { false };
				Genode::ascii_to(src, create_snapshot);
				Genode::String<64> str(Genode::Cstring(src, count));

				if (!create_snapshot) {
					return WRITE_ERR_IO;
				}

				if (!_w.create_snapshot()) {
					out_count = 0;
					return WRITE_OK;
				}

				out_count = count;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Create_snapshot_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<create_snapshot/>")),
			_w(w)
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
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_cbe::Discard_snapshot_file_system : public Vfs::Single_file_system
{
	private:

		Wrapper &_w;

		struct Vfs_handle : Single_vfs_handle
		{
			Wrapper &_w;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Wrapper &w)
			:
				Single_vfs_handle(ds, fs, alloc, 0),
				_w(w)
			{ }

			Read_result read(char *, file_size, file_size &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(char const *src, file_size count,
			                   file_size &out_count) override
			{
				out_count = 0;

				Genode::uint64_t id { 0 };
				Genode::ascii_to(src, id);
				if (id == 0) {
					return WRITE_ERR_IO;
				}

				if (!_w.discard_snapshot(Cbe::Generation { id })) {
					out_count = 0;
					return WRITE_OK;
				}

				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Discard_snapshot_file_system(Wrapper &w)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
			                   Node_rwx::wo(), Xml_node("<discard_snapshot/>")),
			_w(w)
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
					new (alloc) Vfs_handle(*this, *this, alloc, _w);
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

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs_cbe::Snapshot_local_factory : File_system_factory
{
	Data_file_system _block_fs;

	Snapshot_local_factory(Vfs::Env    &env,
	                       Wrapper &cbe,
	                       uint32_t snap_id)
	: _block_fs(cbe, snap_id) { }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Data_file_system::type_name()))
			return &_block_fs;

		return nullptr;
	}
};


class Vfs_cbe::Snapshot_file_system : private Snapshot_local_factory,
                                      public Vfs::Dir_file_system
{
	private:

		Genode::uint32_t _snap_id;

		typedef String<128> Config;

		static Config _config(Genode::uint32_t snap_id, bool readonly)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {

				xml.attribute("name",
				              !readonly ? String<16>("current")
				                        : String<16>(snap_id));

				xml.node("data", [&] () {
					xml.attribute("readonly", readonly);
				});
			});

			return Config(Cstring(buf));
		}

	public:

		Snapshot_file_system(Vfs::Env        &vfs_env,
		                    Wrapper          &cbe,
		                    Genode::uint32_t  snap_id,
		                    bool              readonly = false)
		:
			Snapshot_local_factory(vfs_env, cbe, snap_id),
			Vfs::Dir_file_system(vfs_env,
			                     Xml_node(_config(snap_id, readonly).string()),
			                     *this),
			_snap_id(snap_id)
		{ }

		static char const *type_name() { return "snapshot"; }

		char const *type() override { return type_name(); }

		Genode::uint32_t snapshot_id() const
		{
			return _snap_id;
		}
};


class Vfs_cbe::Snapshots_file_system : public Vfs::File_system
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
			Wrapper                                                    &_wrapper;
			Snapshots_file_system                                      &_snapshots_fs;
			uint32_t                                                    _number_of_snapshots { 0 };
			Genode::Registry<Genode::Registered<Snapshot_file_system>>  _registry            { };

			struct Invalid_index : Genode::Exception { };
			struct Invalid_path  : Genode::Exception { };



			Snapshot_registry(Genode::Allocator     &alloc,
			                  Wrapper               &wrapper,
			                  Snapshots_file_system &snapshots_fs)
			:
				_alloc        { alloc },
				_wrapper      { wrapper },
				_snapshots_fs { snapshots_fs }
			{ }

			void update(Vfs::Env &vfs_env);

			uint32_t number_of_snapshots() const { return _number_of_snapshots; }

			Snapshot_file_system const &by_index(uint32_t idx) const
			{
				uint32_t i = 0;
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

			Snapshot_file_system &_by_id(uint32_t id)
			{
				Snapshot_file_system *fsp { nullptr };
				auto lookup = [&] (Snapshot_file_system &fs) {
					if (fs.snapshot_id() == id) {
						fsp = &fs;
					}
				};
				_registry.for_each(lookup);
				if (fsp == nullptr) {
					throw Invalid_path();
				}
				return *fsp;
			}

			Snapshot_file_system &by_path(char const *path)
			{
				if (!path) {
					throw Invalid_path();
				}

				if (path[0] == '/') {
					path++;
				}

				uint32_t id { 0 };
				Genode::ascii_to(path, id);
				return _by_id(id);
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

			virtual Read_result read(char *dst, file_size count,
			                         file_size &out_count) = 0;

			virtual Write_result write(char const *src, file_size count,
			                           file_size &out_count) = 0;

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

			Read_result _query_snapshots(file_size  index,
			                             file_size &out_count,
			                             Dirent    &out)
			{
				if (index >= _snap_reg.number_of_snapshots()) {
					out_count = sizeof(Dirent);
					out.type = Dirent_type::END;
					return READ_OK;
				}

				try {
					Snapshot_file_system const &fs = _snap_reg.by_index(index);
					Genode::String<32> name { fs.snapshot_id() };

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

			Read_result _query_root(file_size  index,
			                        file_size &out_count,
			                        Dirent    &out)
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

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				out_count = 0;

				if (count < sizeof(Dirent))
					return READ_ERR_INVALID;

				file_size index = seek() / sizeof(Dirent);

				Dirent &out = *(Dirent*)dst;

				if (!_root_dir) {

					/* opended as "/snapshots" */
					return _query_snapshots(index, out_count, out);

				} else {
					/* opened as "/" */
					return _query_root(index, out_count, out);
				}
			}

			Write_result write(char const *, file_size, file_size &) override
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
		Wrapper           &_wrapper;

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
		                      Genode::Xml_node  node,
		                      Wrapper          &wrapper)
		:
			_vfs_env  { vfs_env },
			_snap_reg { vfs_env.alloc(), wrapper, *this },
			_wrapper  { wrapper }
		{
			_wrapper.manage_snapshots_file_system(*this);
		}

		~Snapshots_file_system()
		{
			_wrapper.dissolve_snapshots_file_system(*this);
		}

		static char const *type_name() { return "snapshots"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path)
		{
			return Genode::Dataspace_capability();
		}

		void release(char const *path, Dataspace_capability)
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

		Unlink_result unlink(char const *path)
		{
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const *from, char const *to) override
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

		Write_result write(Vfs::Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			return WRITE_ERR_IO;
		}

		bool queue_read(Vfs::Vfs_handle *vfs_handle, file_size size) override
		{
			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().queue_read(&dh->vfs_handle,
				                                      size);
			}

			return true;
		}

		Read_result complete_read(Vfs::Vfs_handle *vfs_handle,
		                          char *dst, file_size count,
		                          file_size & out_count) override
		{
			Snap_vfs_handle *sh =
				dynamic_cast<Snap_vfs_handle*>(vfs_handle);
			if (sh) {
				Read_result const res = sh->read(dst, count, out_count);
				return res;
			}

			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().complete_read(&dh->vfs_handle,
				                                         dst, count, out_count);
			}

			return READ_ERR_IO;
		}

		bool read_ready(Vfs::Vfs_handle const &) const override
		{
			return true;
		}

		bool write_ready(Vfs::Vfs_handle const &) const override
		{
			return false;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *vfs_handle,
		                           file_size len) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs_cbe::Control_local_factory : File_system_factory
{
	Rekey_file_system             _rekeying_fs;
	Deinitialize_file_system      _deinitialize_fs;
	Create_snapshot_file_system   _create_snapshot_fs;
	Discard_snapshot_file_system  _discard_snapshot_fs;
	Extend_file_system            _extend_fs;

	Control_local_factory(Vfs::Env     &env,
	                      Xml_node      config,
	                      Wrapper      &cbe)
	:
		_rekeying_fs(cbe),
		_deinitialize_fs(cbe),
		_create_snapshot_fs(cbe),
		_discard_snapshot_fs(cbe),
		_extend_fs(cbe)
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Rekey_file_system::type_name())) {
			return &_rekeying_fs;
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

		return nullptr;
	}
};


class Vfs_cbe::Control_file_system : private Control_local_factory,
                                     public Vfs::Dir_file_system
{
	private:

		typedef String<128> Config;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", "control");
				xml.node("rekey", [&] () { });
				xml.node("extend", [&] () { });
				xml.node("create_snapshot", [&] () { });
				xml.node("discard_snapshot", [&] () { });
				xml.node("deinitialize", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		Control_file_system(Vfs::Env         &vfs_env,
		                    Genode::Xml_node  node,
		                    Wrapper          &cbe)
		:
			Control_local_factory(vfs_env, node, cbe),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this)
		{ }

		static char const *type_name() { return "control"; }

		char const *type() override { return type_name(); }
};


struct Vfs_cbe::Local_factory : File_system_factory
{
	Snapshot_file_system _current_snapshot_fs;
	Snapshots_file_system _snapshots_fs;
	Control_file_system _control_fs;

	Local_factory(Vfs::Env &env, Xml_node config,
	              Wrapper &cbe)
	:
		_current_snapshot_fs(env, cbe, 0, false),
		_snapshots_fs(env, config, cbe),
		_control_fs(env, config, cbe)
	{ }

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


class Vfs_cbe::File_system : private Local_factory,
                             public Vfs::Dir_file_system
{
	private:

		Wrapper &_wrapper;

		typedef String<256> Config;

		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				typedef String<64> Name;

				xml.attribute("name",
				              node.attribute_value("name",
				                                   Name("cbe")));

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
		            Wrapper &wrapper)
		:
			Local_factory(vfs_env, node, wrapper),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()),
			                     *this),
			_wrapper(wrapper)
		{ }

		~File_system()
		{
			// XXX rather then destroying the wrapper here, it should be
			//     done on the out-side where it was allocated in the first
			//     place but the factory interface does not support that yet
			// destroy(vfs_env.alloc().alloc()), &_wrapper);
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
				/* XXX wrapper is not managed and will leak */
				Vfs_cbe::Wrapper *wrapper =
					new (vfs_env.alloc()) Vfs_cbe::Wrapper { vfs_env, node };
				return new (vfs_env.alloc())
					Vfs_cbe::File_system(vfs_env, node, *wrapper);
			} catch (...) {
				Genode::error("could not create 'cbe_fs' ");
			}
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}


/**********************
 ** Vfs_cbe::Wrapper **
 **********************/

void Vfs_cbe::Wrapper::_snapshots_fs_update_snapshot_registry()
{
	if (_snapshots_fs.valid()) {
		_snapshots_fs.obj().update_snapshot_registry();
	}
}


void Vfs_cbe::Wrapper::_extend_fs_trigger_watch_response()
{
	if (_extend_fs.valid()) {
		_extend_fs.obj().trigger_watch_response();
	}
}


void Vfs_cbe::Wrapper::_rekey_fs_trigger_watch_response()
{
	if (_rekey_fs.valid()) {
		_rekey_fs.obj().trigger_watch_response();
	}
}


void Vfs_cbe::Wrapper::_deinit_fs_trigger_watch_response()
{
	if (_deinit_fs.valid()) {
		_deinit_fs.obj().trigger_watch_response();
	}
}


/*******************************************************
 ** Vfs_cbe::Snapshots_file_system::Snapshot_registry **
 *******************************************************/

void Vfs_cbe::Snapshots_file_system::Snapshot_registry::update(Vfs::Env &vfs_env)
{
	Cbe::Active_snapshot_ids list { };
	_wrapper.active_snapshot_ids(list);
	bool trigger_watch_response { false };

	/* alloc new */
	for (size_t i = 0; i < sizeof (list.values) / sizeof (list.values[0]); i++) {

		uint32_t const id = list.values[i];
		if (!id) {
			continue;
		}
		bool is_old = false;
		auto find_old = [&] (Snapshot_file_system const &fs) {
			is_old |= (fs.snapshot_id() == id);
		};
		_registry.for_each(find_old);

		if (!is_old) {

			new (_alloc)
				Genode::Registered<Snapshot_file_system> {
					_registry, vfs_env, _wrapper, id, true };

			++_number_of_snapshots;
			trigger_watch_response = true;
		}
	}

	/* destroy old */
	auto find_stale = [&] (Snapshot_file_system const &fs)
	{
		bool is_stale = true;
		for (size_t i = 0; i < sizeof (list.values) / sizeof (list.values[0]); i++) {
			uint32_t const id = list.values[i];
			if (!id) { continue; }

			if (fs.snapshot_id() == id) {
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
