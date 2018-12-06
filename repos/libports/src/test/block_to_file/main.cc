/*
 * \brief  SD-card driver
 * \author Martin Stein
 * \author Sebastian Sumpf
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
#include <base/attached_rom_dataspace.h>
#include <block/component.h>
#include <vfs/simple_env.h>

using namespace Genode;
using Path = Genode::Path<Vfs::MAX_PATH_LEN>;
using Vfs::Directory_service;

namespace Local { class Driver; }


/*************************
 ** Ada interface for C **
 *************************/

extern "C" void ada_block_read(Block::sector_t block_number,
                            size_t          block_size);


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


class Local::Driver : public Block::Driver
{
	private:

		Env &_env;

	public:

		Driver(Env &env) : Block::Driver(env.ram()), _env(env) { }


		/*******************
		 ** Block::Driver **
		 *******************/

		void read(Block::sector_t           block_number,
		          size_t                    block_count,
		          char                     *,
		          Block::Packet_descriptor &pkt) override
		{
			ada_block_read(block_number, block_count);
			ack_packet(pkt);
		}

		void write(Block::sector_t           block_number,
		           size_t                    block_count,
		           char const               *,
		           Block::Packet_descriptor &pkt) override
		{
			log(__func__, " ", block_number, " ", block_count);
			ack_packet(pkt);
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size) override
		{
			log(__func__, " ", size);
			return _env.ram().alloc(size, UNCACHED);
		}

		Genode::size_t block_size() override
		{
			log(__func__);
			return 512;
		}

		Block::sector_t block_count() override
		{
			log(__func__);
			return 1024 * 1024 * 1024 / block_size();
		}

		Block::Session::Operations ops() override
		{
			log(__func__);
			Block::Session::Operations ops;
			ops.set_operation(Block::Packet_descriptor::READ);
			ops.set_operation(Block::Packet_descriptor::WRITE);
			return ops;
		}
};

struct Main
{
	struct Factory : Block::Driver_factory
	{
		Env  &env;
		Heap &heap;

		Factory(Env &env, Heap &heap) : env(env), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Local::Driver(env); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, static_cast<Local::Driver*>(driver)); }
	};

	Env                    &env;
	Heap                    heap            { env.ram(), env.rm() };
	Attached_rom_dataspace  config_rom      { env, "config" };
	Xml_node         const  config_xml      { config_rom.xml() };
	Factory                 factory         { env, heap };
	Block::Root root                        { env.ep(), heap, env.rm(), factory, true };
	Vfs::Simple_env         vfs_env         { env, heap, config_xml.sub_node("vfs") };
	Vfs::File_system       &vfs_root        { vfs_env.root_dir() };

	Main(Genode::Env &env) : env(env)
	{
		Vfs::Vfs_handle *vfs_root_handle { nullptr };
		vfs_root.opendir("/", false, &vfs_root_handle, heap);
		Vfs::Vfs_handle::Guard root_guard(vfs_root_handle);

		::Path path {"/block"};
//		path.append();

		Vfs::Vfs_handle *handle = nullptr;
		assert_open(vfs_root.open(
			path.base(), Directory_service::OPEN_MODE_CREATE, &handle, heap));
		Vfs::Vfs_handle::Guard file_guard(handle);

		Genode::log("--- block to file ---");
		env.parent().announce(env.ep().manage(root));
	}
};

static Main *main_object;

void Component::construct(Genode::Env &env)
{
	static Main _main(env);
	main_object = &_main;
}


/*************************
 ** C interface for Ada **
 *************************/

extern "C" void print_sector(Block::sector_t a)
{
	log("  sector ", a);
}

extern "C" void print_size(size_t a)
{
	log("  size ", a);
}

extern "C" void print_string(char const *str)
{
	log(str);
}
