/*
 * \brief   Dummy server for measuring communication performance
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <root/component.h>

/* local includes */
#include <dummy_session/connection.h>

namespace Dummy
{
	using Server::Entrypoint;
	using Genode::Rpc_object;
	using Genode::env;
	using Genode::Root_component;
	using Genode::Allocator;

	class  Session_component;
	class  Root;
	struct Main;
}

class Dummy::Session_component : public Rpc_object<Session>
{
	public:

		int dummy_rpc(int x) override { return ~x; }
};

class Dummy::Root : public Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args)
		{
			try { return new (md_alloc()) Session_component(); }
			catch (...) { throw Root::Exception(); }
		}

	public:

		Root(Entrypoint & ep, Allocator & md_alloc)
		: Root_component<Session_component>(&ep.rpc_ep(), &md_alloc) { }
};

struct Dummy::Main
{
	Server::Entrypoint & ep;

	Root root;

	Main(Server::Entrypoint & ep) : ep(ep), root(ep, *env()->heap()) {
		env()->parent()->announce(ep.manage(root)); }
};


/************
 ** Server **
 ************/

namespace Server
{
	using namespace Dummy;

	char const *name() { return "dummy_ep"; }

	size_t stack_size() { return 2 * 1024 * sizeof(long); }

	void construct(Entrypoint & ep) { static Main main(ep); }
}
