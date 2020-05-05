/*
 * \brief  VFS file to Block session
 * \author Josef Soentgen
 * \date   2020-05-05
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block/request_stream.h>
#include <util/string.h>
#include <vfs/simple_env.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>

/* local includes */
#include "job.h"


using namespace Genode;


namespace Vfs_block {

	class File;

} /* namespace Vfs_block */


class Vfs_block::File
{
	private:

		File(const File&) = delete;
		File& operator=(const File&) = delete;

		Vfs::File_system &_vfs;
		Vfs::Vfs_handle  *_vfs_handle;

		bool _verbose;

		Constructible<Vfs_block::Job> _job { };

		struct Io_response_handler : Vfs::Io_response_handler
		{
			Signal_context_capability sigh { };

			void read_ready_response() override { }

			void io_progress_response() override
			{
				if (sigh.valid()) {
					Signal_transmitter(sigh).submit();
				}
			}
		};
		Io_response_handler _io_response_handler { };

		Block::Session::Info _info { };

	public:

		File(Genode::Xml_node const    &config,
		           Genode::Allocator         &alloc,
		           Vfs::File_system          &vfs,
		           Signal_context_capability  sigh)
		:
			_vfs        { vfs },
			_vfs_handle { nullptr },
			_verbose    { config.attribute_value("verbose", false) }
		{
			using File_name = Genode::String<64>;
			File_name const file_name =
				config.attribute_value("file", File_name());
			if (!file_name.valid()) {
				Genode::error("config 'file' attribute invalid");
				throw Genode::Exception();
			}

			bool const writeable =
				config.attribute_value("writeable", false);

			unsigned const mode =
				writeable ? Vfs::Directory_service::OPEN_MODE_RDWR
				          : Vfs::Directory_service::OPEN_MODE_RDONLY;

			using Open_result = Vfs::Directory_service::Open_result;
			Open_result res = _vfs.open(file_name.string(), mode,
			                            &_vfs_handle, alloc);
			if (res != Open_result::OPEN_OK) {
				error("Could not open '", file_name.string(), "'");
				throw Genode::Exception();
			}

			using Stat_result = Vfs::Directory_service::Stat_result;
			Vfs::Directory_service::Stat stat { };
			Stat_result stat_res = _vfs.stat(file_name.string(), stat);
			if (stat_res != Stat_result::STAT_OK) {
				_vfs.close(_vfs_handle);
				error("Could not stat '", file_name.string(), "'");
				throw Genode::Exception();
			}

			size_t const block_size =
				config.attribute_value("block_size", 512u);
			Block::block_number_t const block_count = stat.size / block_size;

			_info = Block::Session::Info {
				.block_size  = block_size,
				.block_count = block_count,
				.align_log2  = log2(block_size),
				.writeable   = true,
			};

			_io_response_handler.sigh = sigh;
			_vfs_handle->handler(&_io_response_handler);

			log("Provide Block session for file '", file_name.string(),
			    "' with block count: ", _info.block_count,
			    " block size: ", _info.block_size);
		}

		~File()
		{
			/*
			 * Sync is expected to be done through the Block
			 * request stream, omit it here.
			 */
			_vfs.close(_vfs_handle);
		}

		Block::Session::Info info() const { return _info; }

		bool execute()
		{
			if (!_job.constructed()) {
				return false;
			}

			if (_verbose) {
				log("Execute: ", _job);
			}

			return _job->execute();
		}

		bool acceptable() const
		{
			return !_job.constructed();
		}

		bool valid(Block::Request const &request)
		{
			using Type = Block::Operation::Type;

			/*
 			 * For READ/WRITE requests we need a valid block count
			 * and number. Other requests might not provide such
			 * information because it is not needed.
			 */

			Block::Operation const op = request.operation;
			switch (op.type) {
			case Type::READ: [[fallthrough]];
			case Type::WRITE:
				return op.count
				    && (op.block_number + op.count) <= _info.block_count;

			case Type::SYNC: return true;

			/* not supported for now */
			case Type::TRIM: [[fallthrough]];
			default:         return false;
			}
		}

		void submit(Block::Request req, void *ptr, size_t length)
		{
			file_offset const base_offset =
				req.operation.block_number * _info.block_size;

			_job.construct(*_vfs_handle, req, base_offset,
			               reinterpret_cast<char*>(ptr), length);

			if (_verbose) {
				log("Submit: ", _job);
			}
		}

		template <typename FN>
		void with_any_completed_job(FN const &fn)
		{
			if (!_job.constructed() || !_job->completed()) {
				return;
			}

			if (_verbose) {
				log("Complete: ", _job);
			}

			Block::Request req = _job->request;
			req.success = _job->succeeded();

			_job.destruct();

			fn(req);
		}
};


struct Block_session_component : Rpc_object<Block::Session>,
                                 private Block::Request_stream
{
	Entrypoint &_ep;

	using Block::Request_stream::with_requests;
	using Block::Request_stream::with_content;
	using Block::Request_stream::try_acknowledge;
	using Block::Request_stream::wakeup_client_if_needed;

	Block_session_component(Region_map                 &rm,
	                        Dataspace_capability        ds,
	                        Entrypoint                 &ep,
	                        Signal_context_capability   sigh,
	                        Block::Session::Info const &info)
	:
		Request_stream(rm, ds, ep, sigh, info),
		_ep(ep)
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	Info info() const override { return Request_stream::info(); }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};


struct Main : Rpc_object<Typed_root<Block::Session>>
{
	Env &_env;

	Signal_handler<Main> _request_handler {
		_env.ep(), *this, &Main::_handle_requests };

	Heap                    _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env { _env, _heap,
		_config_rom.xml().sub_node("vfs") };

	Vfs_block::File _block_file { _config_rom.xml(), _heap,
		_vfs_env.root_dir(), _request_handler };

	Constructible<Attached_ram_dataspace>  _block_ds { };
	Constructible<Block_session_component> _block_session { };

	void _handle_requests()
	{
		if (!_block_session.constructed()) {
			return;
		}

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			block_session.with_requests([&] (Block::Request request) {

				using Response = Block::Request_stream::Response;

				if (!_block_file.acceptable()) {
					return Response::RETRY;
				}

				if (!_block_file.valid(request)) {
					return Response::REJECTED;
				}

				bool const payload =
					Block::Operation::has_payload(request.operation.type);

				try {
					if (payload) {
						block_session.with_content(request,
						[&] (void *ptr, size_t size) {
							_block_file.submit(request, ptr, size);
						});
					} else {
						_block_file.submit(request, nullptr, 0);
					}
				} catch (Vfs_block::Job::Unsupported_Operation) {
					return Response::REJECTED;
				}

				progress |= true;
				return Response::ACCEPTED;
			});

			progress |= _block_file.execute();

			block_session.try_acknowledge([&] (Block::Request_stream::Ack &ack) {

				_block_file.with_any_completed_job([&] (Block::Request request) {
					ack.submit(request);
					progress |= true;
				});
			});

			if (!progress) { break; }
		}

		block_session.wakeup_client_if_needed();
	}


	/*
	 * Root interface
	 */

	Capability<Session> session(Root::Session_args const &args,
	                            Affinity const &) override
	{
		log("new block session: ", args.string());

		size_t const ds_size =
			Arg_string::find_arg(args.string(), "tx_buf_size").ulong_value(0);

		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		if (ds_size >= ram_quota.value) {
			warning("communication buffer size exceeds session quota");
			throw Insufficient_ram_quota();
		}

		_block_ds.construct(_env.ram(), _env.rm(), ds_size);
		_block_session.construct(_env.rm(), _block_ds->cap(), _env.ep(),
		                         _request_handler, _block_file.info());

		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session>) override
	{
		_block_session.destruct();
		_block_ds.destruct();
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(*this));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
