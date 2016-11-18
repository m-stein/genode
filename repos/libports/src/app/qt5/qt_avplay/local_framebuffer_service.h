/*
 * \brief   Framebuffer service provided to avplay
 * \author  Christian Prochaska
 * \date    2016-11-17
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#ifndef _LOCAL_FRAMEBUFFER_SERVICE_H_
#define _LOCAL_FRAMEBUFFER_SERVICE_H_

/* Genode includes */
#include <root/component.h>

#include "framebuffer_session_component.h"

namespace Framebuffer {
	class Local_framebuffer_factory;
	using namespace Genode;
}

typedef Genode::Local_service<Framebuffer::Session_component> Framebuffer_service;

class Framebuffer::Local_framebuffer_factory : public Framebuffer_service::Factory
{
	private:

		Genode::Env            &_env;
		Genode::Rpc_entrypoint &_ep;
		QNitpickerViewWidget   &_nitpicker_view_widget;
		int                     _max_width;
		int                     _max_height;

	public:

		Local_framebuffer_factory(Genode::Env &env,
		                          Genode::Rpc_entrypoint &ep,
		                          QNitpickerViewWidget &nitpicker_view_widget,
		                          int max_width = 0,
		                          int max_height = 0)
		: _env(env),
		  _ep(ep),
		  _nitpicker_view_widget(nitpicker_view_widget),
		  _max_width(max_width),
		  _max_height(max_height)
		{ }

		Framebuffer::Session_component &create(Args const &args, Affinity) override
		{
			return *new (env()->heap())
				Framebuffer::Session_component(_env, _ep, args,
				                               _nitpicker_view_widget,
				                               _max_width, _max_height);
		}

		void upgrade(Framebuffer::Session_component &, Args const &) override { }

		void destroy(Framebuffer::Session_component &session) override
		{
			Genode::destroy(env()->heap(), &session);
		}
};

#endif /* _LOCAL_FRAMEBUFFER_SERVICE_H_ */
