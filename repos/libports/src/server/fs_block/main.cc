/*
 * \brief  SD-card driver
 * \author Martin Stein
 * \date   2013-03-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <block/request_stream.h>
#include <vfs/simple_env.h>

using namespace Genode;
using Path = Genode::Path<Vfs::MAX_PATH_LEN>;
using Vfs::Directory_service;

namespace Local
{
	class Main;
	class Block_session_component;
}

struct Local::Block_session_component : Rpc_object<Block::Session>,
                                       Block::Request_stream
{
	Entrypoint &_ep;

	static constexpr size_t BLOCK_SIZE = 4096;
	static constexpr size_t NUM_BLOCKS = 16;

	Block_session_component(Region_map               &rm,
	                        Dataspace_capability      ds,
	                        Entrypoint               &ep,
	                        Signal_context_capability sigh)
	:
		Request_stream(rm, ds, ep, sigh), _ep(ep)
	{
		_ep.manage(*this);
	}

	~Block_session_component() { _ep.dissolve(*this); }

	void info(Block::sector_t *count, size_t *block_size, Operations *ops) override
	{
		*count      = NUM_BLOCKS;
		*block_size = BLOCK_SIZE;
		*ops        = Operations();

		ops->set_operation(Block::Packet_descriptor::Opcode::READ);
		ops->set_operation(Block::Packet_descriptor::Opcode::WRITE);
	}

	void sync() override { }

	Capability<Tx> tx_cap() override { return Request_stream::tx_cap(); }
};

namespace Spark {

	/**
	 * Opaque object that contains the space needed to store a SPARK record.
	 *
	 * \param BYTES  size of the SPARK record in bytes
	 */
	template <Genode::size_t BYTES>
	struct Object
	{
		long _space[(BYTES + sizeof(long) - 1)/sizeof(long)] { };
	};

	struct Fs_block : Object<20480>
	{
		Fs_block(size_t size = sizeof(Fs_block));

		bool acceptable() const;

		void submit(Block::Request &request);

		bool execute()
		{
			static bool x = false;
			x = !x;
			return x;
		}

		void completed_job(Block::Request &)
		{
		}

		/**
		 * Apply 'fn' with completed job, reset job
		 */
		template <typename FN>
		void with_any_completed_job(FN const &)
		{
			return;
		}
	};
}



///*************************
// ** Ada interface for C **
// *************************/
//
//extern "C" void ada_block_read(Spark::App_block_object         &object,
//                               Genode::off_t                    offset,
//                               Genode::size_t                   size,
//                               Block::Packet_descriptor::Opcode op,
//                               Block::sector_t                  block_number,
//                               Genode::size_t                   block_count,
//                               bool                             success);


/***************
 ** Utilities **
 ***************/

inline void assert_open(Vfs::Directory_service::Open_result r)
{
	typedef Vfs::Directory_service::Open_result Result;
	switch (r) {
	case Result::OPEN_OK: return;
	case Result::OPEN_ERR_NAME_TOO_LONG:
		error("OPEN_ERR_NAME_TOO_LONG"); break;
	case Result::OPEN_ERR_UNACCESSIBLE:
		error("OPEN_ERR_UNACCESSIBLE"); break;
	case Result::OPEN_ERR_NO_SPACE:
		error("OPEN_ERR_NO_SPACE"); break;
	case Result::OPEN_ERR_NO_PERM:
		error("OPEN_ERR_NO_PERM"); break;
	case Result::OPEN_ERR_EXISTS:
		error("OPEN_ERR_EXISTS"); break;
	case Result::OPEN_ERR_OUT_OF_RAM:
		error("OPEN_ERR_OUT_OF_RAM"); break;
	case Result::OPEN_ERR_OUT_OF_CAPS:
		error("OPEN_ERR_OUT_OF_CAPS"); break;
	}
	throw Exception();
}


//class Local::Driver : public Block::Driver
//{
//	private:
//
//		Env                     &_env;
//		Spark::App_block_object  _app_block_object { };
//
//	public:
//
//		Driver(Env &env) : Block::Driver(env.ram()), _env(env) { }
//
//
//		/*******************
//		 ** Block::Driver **
//		 *******************/
//
//		void read(Block::sector_t           block_number,
//		          size_t                    block_count,
//		          char                     *,
//		          Block::Packet_descriptor &pkt) override
//		{
//			ada_block_read(_app_block_object,
//			               pkt.offset(),
//			               pkt.size(),
//			               pkt.operation(),
//			               block_number,
//			               block_count,
//			               pkt.succeeded());
//			ack_packet(pkt);
//		}
//
//		void write(Block::sector_t           block_number,
//		           size_t                    block_count,
//		           char const               *,
//		           Block::Packet_descriptor &pkt) override
//		{
//			log(__func__, " ", block_number, " ", block_count);
//			ack_packet(pkt);
//		}
//
//		Ram_dataspace_capability alloc_dma_buffer(size_t size) override
//		{
//			log(__func__, " ", size);
//			return _env.ram().alloc(size, UNCACHED);
//		}
//
//		Genode::size_t block_size() override
//		{
//			log(__func__);
//			return 512;
//		}
//
//		Block::sector_t block_count() override
//		{
//			log(__func__);
//			return 1024 * 1024 * 1024 / block_size();
//		}
//
//		Block::Session::Operations ops() override
//		{
//			log(__func__);
//			Block::Session::Operations ops;
//			ops.set_operation(Block::Packet_descriptor::READ);
//			ops.set_operation(Block::Packet_descriptor::WRITE);
//			return ops;
//		}
//};

struct Local::Main : Rpc_object<Typed_root<Block::Session> >
{
	Env                    &_env;
	Heap                    _heap            { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom      { _env, "config" };
	Xml_node         const  _config_xml      { _config_rom.xml() };

	Vfs::Simple_env         _vfs_env         { _env, _heap, _config_xml.sub_node("vfs") };
	Vfs::File_system       &_vfs_root        { _vfs_env.root_dir() };

	Constructible<Attached_ram_dataspace>  _block_ds              { };
	Constructible<Block_session_component> _block_session         { };
	Signal_handler<Main>                   _block_request_handler { _env.ep(), *this, &Main::_handle_block_requests };
	Spark::Fs_block                        _fs_block              { };

	void _handle_block_requests()
	{
log("-----------------", __func__,__LINE__);
		if (!_block_session.constructed())
			return;

		Block_session_component &block_session = *_block_session;

		for (;;) {

			bool progress = false;

			/* import new requests */
			block_session.with_requests([&] (Block::Request request) {

				if (!_fs_block.acceptable())
					return Block_session_component::Response::RETRY;

log(__func__,__LINE__, " ", &request, " ", sizeof(request), " ", (unsigned)request.operation, " ", (unsigned)request.success, " ", (unsigned long long)request.offset, " ", (unsigned)request.size);
				_fs_block.submit(request);

				progress = true;

				return Block_session_component::Response::ACCEPTED;
			});

			/* process I/O */
			progress |= _fs_block.execute();

			/* acknowledge finished jobs */
			block_session.try_acknowledge([&] (Block_session_component::Ack &ack) {

				_fs_block.with_any_completed_job([&] (Block::Request request) {
					progress |= true;
					ack.submit(request);
				});
			});

			if (!progress)
				break;
		}

		block_session.wakeup_client();
	}

	Main(Genode::Env &env) : _env(env)
	{
		Vfs::Vfs_handle *vfs_root_handle { nullptr };
		_vfs_root.opendir("/", false, &vfs_root_handle, _heap);
		Vfs::Vfs_handle::Guard root_guard(vfs_root_handle);

		::Path path {"/block"};
		Vfs::Vfs_handle *handle = nullptr;
		assert_open(_vfs_root.open(
			path.base(), Directory_service::OPEN_MODE_CREATE, &handle, _heap));
		Vfs::Vfs_handle::Guard file_guard(handle);

		Genode::log("--- block to file ---");
		_env.parent().announce(_env.ep().manage(*this));
	}


	/*********************************************
	 ** Rpc_object<Typed_root<Block::Session> > **
	 *********************************************/

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
		                         _block_request_handler);

		return _block_session->cap();
	}

	void upgrade(Capability<Session>, Root::Upgrade_args const &) override { }

	void close(Capability<Session>) override
	{
		_block_session.destruct();
		_block_ds.destruct();
	}
};

void Component::construct(Genode::Env &env) { static Local::Main main(env); }


/*************************
 ** C interface for Ada **
 *************************/

extern "C" void c_genode_log_uint64(uint64_t v) { log(v); }
extern "C" void c_genode_log_unsigned_long(unsigned long v) { log(v); }
extern "C" void c_genode_log              (char const *v)   { log(v); }
extern "C" void c_genode_error            (char const *v)   { error(v); }
