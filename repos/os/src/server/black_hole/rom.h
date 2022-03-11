/*
 * \brief  ROM session component and root
 * \author Martin Stein
 * \date   2022-03-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROM_H_
#define _ROM_H_

/* base includes */
#include <base/attached_ram_dataspace.h>
#include <rom_session/rom_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class  Rom_session;
	class  Rom_root;
}


class Black_hole::Rom_session : public Rpc_object<Genode::Rom_session>
{
	private:

		Env                    &_env;
		Attached_ram_dataspace  _ram_ds { _env.ram(), _env.rm(), 32 };

	public:

		Rom_session(Env &env) : _env(env) { }

		Rom_dataspace_capability dataspace() override
		{
			String<32> const content { "<empty/>" };
			memcpy(
				_ram_ds.local_addr<char>(), content.string(), content.size());

			Dataspace_capability ds_cap {
				static_cap_cast<Dataspace>(_ram_ds.cap()) };

			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		void sigh(Signal_context_capability /* sigh */) override { }
};


class Black_hole::Rom_root : public Root_component<Rom_session>
{
	private:

		Env &_env;

	protected:

		Rom_session *_create_session(const char * /* args */) override
		{
			return new (md_alloc()) Rom_session { _env };
		}

	public:

		Rom_root(Env &env, Allocator &md_alloc)
		:
			Root_component<Rom_session> { &env.ep().rpc_ep(), &md_alloc },
			_env                        { env }
		{ }
};

#endif /* _ROM_H_ */
