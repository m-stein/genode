/*
 * \brief   Filter framebuffer policy
 * \author  Christian Prochaska
 * \date    2012-04-11
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILTER_FRAMEBUFFER_SLAVE_H_
#define _FILTER_FRAMEBUFFER_SLAVE_H_

/* Genode includes */
#include <base/service.h>
#include <os/slave.h>

/* local includes */
#include "local_framebuffer_service.h"


class Filter_framebuffer_slave
{
	private:

		class Policy : public Genode::Slave::Policy
		{
			private:

				Framebuffer_service &_framebuffer_service;

			protected:

				const char **_permitted_services() const
				{
					static const char *permitted_services[] = {
						"LOG", "RM", "ROM", "Timer", 0 };

					return permitted_services;
				};

			public:

				Policy(Genode::Rpc_entrypoint         &entrypoint,
				       Genode::Region_map             &rm,
				       Genode::Ram_session_capability  ram,
				       Name const                     &name,
				       size_t                          quota,
				       Framebuffer_service            &framebuffer_service)
				: Genode::Slave::Policy(name, name, entrypoint, rm, ram, quota),
		  	  	  _framebuffer_service(framebuffer_service) { }

				Genode::Service &resolve_session_request(Genode::Service::Name       const &service_name,
		                                         	 	 Genode::Session_state::Args const &args) override
				{
					if (service_name == "Framebuffer")
						return _framebuffer_service;

					return Genode::Slave::Policy::resolve_session_request(service_name, args);
				}
		};

		Genode::size_t   const _ep_stack_size = 2*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 */
		Filter_framebuffer_slave(Genode::Pd_session                &pd,
		                         Genode::Region_map                &rm,
		                         Genode::Ram_session_capability     ram,
		                         Genode::Slave::Policy::Name const &name,
		                         size_t                             quota,
		                         Framebuffer_service               &framebuffer_service)
		:
			_ep(&pd, _ep_stack_size, "filter_framebuffer_ep"),
			_policy(_ep, rm, ram, name, quota, framebuffer_service),
			_child(rm, _ep, _policy)
		{ Genode::log(__PRETTY_FUNCTION__, ": ", &_child); }

};

#endif /* _FILTER_FRAMEBUFFER_SLAVE_H_ */
