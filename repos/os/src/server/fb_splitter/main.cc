/*
 * \brief  Framebuffer driver for Exynos5 HDMI
 * \author Martin Stein
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/connection.h>
#include <base/printf.h>
#include <os/server.h>
#include <root/component.h>

namespace Framebuffer
{
	using namespace Genode;
	class Session_component;
	class Root;

	enum { MAX_LABEL_LEN = 128 };
	typedef Genode::String<MAX_LABEL_LEN> Label;
	typedef Genode::Root_component<Session_component, Genode::Multiple_clients> Root_component;
}

class Framebuffer::Session_component
:
	public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Framebuffer::Connection & _fb_con;
		Genode::Signal_context_capability _mode_sigh;
		Genode::Signal_context_capability _sync_sigh;

	public:

		Label label;

		Session_component(char const * label, Framebuffer::Connection & fb_con)
		: _fb_con(fb_con), label(label) { }

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override {
			return _fb_con.dataspace(); }

		Mode mode() const override { return _fb_con.mode(); }

		void mode_sigh(Genode::Signal_context_capability sigh) override
		{
			/* FIXME: done merely for NOVA and SEL4 */
			_mode_sigh = sigh;
			_fb_con.mode_sigh(sigh);
		}

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			/* FIXME: done merely for NOVA and SEL4 */
			_sync_sigh = sigh;
			_fb_con.sync_sigh(sigh);
		}

		void refresh(int a, int b, int c, int d) override {
			_fb_con.refresh(a,b,c,d); }
};

class Framebuffer::Root : public Framebuffer::Root_component
{
	private:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			char label[MAX_LABEL_LEN];
			Arg_string::find_arg(args, "label").string(label,
			                                           sizeof(label),
			                                           "<noname>");

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t session_size = align_addr(sizeof(Session_component), 12);

			if (ram_quota < session_size) {
				PERR("insufficient 'ram_quota', got %zu, need %zu",
				     ram_quota, session_size);
				throw Root::Quota_exceeded();
			}

			static Framebuffer::Connection fb_con;
			Session_component *session = new (md_alloc())
				Session_component(label, fb_con);

			return session;

		}

		void _destroy_session(Session_component *session) {
			destroy(md_alloc(), session); }

	public:

		Root(Server::Entrypoint &ep, Genode::Allocator &md_alloc)
		: Root_component(&ep.rpc_ep(), &md_alloc) { }
};


/************
 ** Server **
 ************/

namespace Server { struct Main; }

struct Server::Main
{
	Server::Entrypoint &ep;

	Framebuffer::Root root = { ep, *Genode::env()->heap() };

	Main(Server::Entrypoint &ep) : ep(ep) {
		Genode::env()->parent()->announce(ep.manage(root)); }
};

namespace Server {

	char const *name() { return "fb_splitter_ep"; }

	size_t stack_size() { return 4 * 1024 * sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Main server(ep); }
}
