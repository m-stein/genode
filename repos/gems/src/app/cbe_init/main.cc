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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <block_session/connection.h>
#include <os/path.h>
#include <vfs/dir_file_system.h>
#include <vfs/file_system_factory.h>
#include <vfs/simple_env.h>

/* CBE includes */
#include <cbe/init/configuration.h>
#include <block_allocator.h>
#include <block_io.h>
#include <crypto.h>
#include <ft_initializer.h>
#include <sb_initializer.h>
#include <trust_anchor.h>
#include <vbd_initializer.h>


enum { VERBOSE = 0 };

using namespace Genode;
using namespace Cbe;

static Block_allocator *_block_allocator_ptr;


Genode::uint64_t block_allocator_first_block()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->first_block();
}


Genode::uint64_t block_allocator_nr_of_blks()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->nr_of_blks();
}


class Main : Vfs::Env::User, public Cbe::Module
{
	private:

		/*
		 * Noncopyable
		 */
		Main(Main const &) = delete;
		Main &operator = (Main const &) = delete;

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };

		Attached_rom_dataspace _config_rom { _env, "config" };

		Vfs::Simple_env       _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Vfs::File_system     &_vfs     { _vfs_env.root_dir() };
		Signal_handler<Main>  _sigh    { _env.ep(), *this, &Main::_execute };

		Constructible<Cbe_init::Configuration> _cfg { };

		Trust_anchor    _trust_anchor    { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto          _crypto          { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io        _block_io        { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Block_allocator _block_allocator { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer  _ft_initializer  { };
		Sb_initializer  _sb_initializer  { };

		Module *_module_ptrs[MAX_MODULE_ID + 1] { };

		/**
		 * Vfs::Env::User interface
		 */
		void wakeup_vfs_user() override { _sigh.local_submit(); }

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
								module_name(id), " ", req.src_request_id_str(),
								" --", req, "-| ",
								module_name(req.dst_module_id()));

						return Module::REQUEST_NOT_HANDLED;
					}
					dst_module.submit_request(req);

					if (VERBOSE_MODULE_COMMUNICATION)
						Genode::log(
							module_name(id), " ", req.src_request_id_str(),
							" --", req, "--> ",
							module_name(req.dst_module_id()), " ",
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
							module_name(req.src_module_id()), " ",
							req.src_request_id_str(), " <--", req,
							"-- ", module_name(id), " ",
							req.dst_request_id_str());
					Module &src_module { *_module_ptrs[req.src_module_id()] };
					src_module.generated_request_complete(req);
					progress = true;
				});
			}
		}

		void _execute()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				_modules_execute(progress);
			}

			_vfs_env.io().commit();

			if (_state == COMPLETE)
				_env.parent().exit(0);
		}

		/****************
		 ** Module API **
		 ****************/

		enum State { INVALID, PENDING, IN_PROGRESS, COMPLETE };

		State _state { INVALID };

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (_state != PENDING)
				return false;

			Sb_initializer_request::create(
				buf_ptr, buf_size, COMMAND_POOL, 0,
				(unsigned long)Sb_initializer_request::INIT,
				nullptr, 0,
				_cfg->vbd_nr_of_lvls() - 1,
				_cfg->vbd_nr_of_children(),
				_cfg->vbd_nr_of_leafs(),
				_cfg->ft_nr_of_lvls() - 1,
				_cfg->ft_nr_of_children(),
				_cfg->ft_nr_of_leafs(),
				_cfg->ft_nr_of_lvls() - 1,
				_cfg->ft_nr_of_children(),
				_cfg->ft_nr_of_leafs());

			return true;
		}

		void _drop_generated_request(Module_request &mod_req) override
		{
			if (_state != PENDING) {
				class Exception_1 { };
				throw Exception_1 { };
			}

			switch (mod_req.dst_module_id()) {
			case SB_INITIALIZER:
				_state = IN_PROGRESS;
				break;
			default:
				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void generated_request_complete(Module_request &mod_req) override
		{
			if (_state != IN_PROGRESS) {
				class Exception_1 { };
				throw Exception_1 { };
			}

			switch (mod_req.dst_module_id()) {
			case SB_INITIALIZER:
				_state = COMPLETE;
				break;
			default:
				class Exception_2 { };
				throw Exception_2 { };
			}
		}

	public:

		Main(Env &env) : _env { env }
		{
			_modules_add(COMMAND_POOL,      *this);
			_modules_add(CRYPTO,            _crypto);
			_modules_add(TRUST_ANCHOR,      _trust_anchor);
			_modules_add(BLOCK_IO,          _block_io);
			_modules_add(BLOCK_ALLOCATOR,   _block_allocator);
			_modules_add(VBD_INITIALIZER,   _vbd_initializer);
			_modules_add(FT_INITIALIZER,    _ft_initializer);
			_modules_add(SB_INITIALIZER,    _sb_initializer);

			_block_allocator_ptr = &_block_allocator;

			Xml_node const &config { _config_rom.xml() };
			try {
				_cfg.construct(config);
				_state = PENDING;

				_execute();
			}
			catch (Cbe_init::Configuration::Invalid) {
				error("bad configuration");
				_env.parent().exit(-1);
			}
		}
};


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	static Main main(env);
}


/*
 * XXX Libc::Component::construct is needed for linking libcrypto
 *     because it depends on the libc but does not need to be
 *     executed.
 */
namespace Libc {
	struct Env;

	struct Component
	{
		void construct(Libc::Env &);
	};
}


void Libc::Component::construct(Libc::Env &) { }
