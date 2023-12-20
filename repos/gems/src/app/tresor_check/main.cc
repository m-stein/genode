/*
 * \brief  Verify the dimensions and hashes of a tresor container
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
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/ft_check.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>

using namespace Genode;
using namespace Tresor;

namespace Tresor_check { class Main; }

class Tresor_check::Main : private Vfs::Env::User, private Tresor::Module_composition, public  Tresor::Module, public Module_channel
{
	private:

		enum State { INIT, REQ_GENERATED, CHECK_SB_SUCCEEDED };

		Env  &_env;
		Heap  _heap { _env.ram(), _env.rm() };
		Attached_rom_dataspace _config_rom { _env, "config" };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _sigh { _env.ep(), *this, &Main::_handle_signal };
		Trust_anchor _trust_anchor { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto _crypto { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io _blk_io { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Vbd_check _vbd_chk { };
		Ft_check _ft_chk { };
		Sb_check _sb_chk { };
		bool _generated_req_success { };
		State _generated_req_succeeded { INIT };
		State _state { INIT };
		Constructible<Sb_check::Check> _chk_sb { };

		NONCOPYABLE(Main);

		void _generated_req_completed(State_uint state_uint) override
		{
			if (!_generated_req_success) {
				error("command pool: request failed because generated request failed)");
				_env.parent().exit(-1);
				return;
			}
			_state = (State)state_uint;
		}

		void wakeup_vfs_user() override { _sigh.local_submit(); }

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			execute_modules();
			_wakeup_back_end_services();
		}

		template <typename REQUEST, typename... ARGS>
		void _generate_req(Constructible<REQUEST> &req, State req_succeeded, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			req.construct(typename REQUEST::Attr { args..., _generated_req_success });
			_generated_req_succeeded = req_succeeded;
			progress = true;
		}

		void _execute_generated_req(Sb_check &sb_chk, Vbd_check &vbd_chk, Ft_check &ft_chk, Block_io &blk_io, bool &progress)
		{
			if (_state != REQ_GENERATED)
				return;

			bool complete { false };
			if (_chk_sb.constructed()) {
				progress |= sb_chk.execute_check(*_chk_sb, vbd_chk, ft_chk, blk_io);
				complete = _chk_sb->complete();
				if (complete)
					_chk_sb.destruct();
			}
			if (complete) {
				if (!_generated_req_success) {
					error("request failed: generated request failed");
					_env.parent().exit(-1);
					return;
				}
				_state = _generated_req_succeeded;
				progress = true;
			}
		}

	public:

		Main(Env &env) : Module_channel(COMMAND_POOL, 0), _env(env)
		{
			add_module(COMMAND_POOL, *this);
			add_module(CRYPTO, _crypto);
			add_module(TRUST_ANCHOR, _trust_anchor);
			add_module(BLOCK_IO, _blk_io);
			add_channel(*this);
			_handle_signal();
		}

		void execute(bool &progress) override
		{
			_execute_generated_req(_sb_chk, _vbd_chk, _ft_chk, _blk_io, progress);
			switch(_state) {
			case INIT: _generate_req(_chk_sb, CHECK_SB_SUCCEEDED, progress); break;
			case CHECK_SB_SUCCEEDED: _env.parent().exit(0); break;
			default: break;
			}
		}
};

void Component::construct(Genode::Env &env) { static Tresor_check::Main main { env }; }

namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
