/*
 * \brief  Slave used for presenting the menu
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MENU_VIEW_SLAVE_H_
#define _MENU_VIEW_SLAVE_H_

/* Genode includes */
#include <os/slave.h>
#include <nitpicker_session/nitpicker_session.h>

/* gems includes */
#include <os/single_session_service.h>

/* local includes */
#include <types.h>

namespace Launcher { class Menu_view_slave; }


class Launcher::Menu_view_slave
{
	public:

		typedef Surface_base::Point Position;

	private:

		class Policy : public Genode::Slave::Policy
		{
			private:

				Genode::Single_session_service<Nitpicker::Session>  _nitpicker;
				Genode::Single_session_service<Genode::Rom_session> _dialog_rom;
				Genode::Single_session_service<Report::Session>     _hover_report;

				Position _position;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"CPU", "PD", "RAM", "ROM", "LOG", "Timer", 0 };

					return permitted_services;
				};

			private:

				void _configure(Position pos)
				{
					char config[1024];

					snprintf(config, sizeof(config),
					         "<config xpos=\"%d\" ypos=\"%d\">\n"
					         "  <report hover=\"yes\"/>\n"
					         "  <libc>\n"
					         "    <vfs>\n"
					         "      <tar name=\"menu_view_styles.tar\" />\n"
					         "    </vfs>\n"
					         "  </libc>\n"
					         "</config>",
					         pos.x(), pos.y());

					configure(config);
				}

				static Name           _name()  { return "menu_view"; }
				static Genode::size_t _quota() { return 6*1024*1024; }

			public:

				Policy(Genode::Rpc_entrypoint        &ep,
				       Genode::Region_map            &rm,
				       Genode::Ram_session_capability ram,
				       Capability<Nitpicker::Session> nitpicker_session,
				       Capability<Rom_session>        dialog_rom_session,
				       Capability<Report::Session>    hover_report_session,
				       Position                       position)
				:
					Genode::Slave::Policy(_name(), _name(), ep, rm, ram, _quota()),
					_nitpicker(nitpicker_session),
					_dialog_rom(dialog_rom_session),
					_hover_report(hover_report_session),
					_position(position)
				{
					_configure(position);
				}

				void position(Position pos) { _configure(pos); }

				Genode::Service &resolve_session_request(Genode::Service::Name const &service,
				                                         Genode::Session_state::Args const &args) override
				{
					if (service == "Nitpicker")
						return _nitpicker.service();

					Genode::Session_label const label(label_from_args(args.string()));

					if ((service == "ROM") && (label == "menu_view -> dialog"))
						return _dialog_rom.service();

					if ((service == "Report") && (label == "menu_view -> hover"))
						return _hover_report.service();

					return Genode::Slave::Policy::resolve_session_request(service, args);
				}
		};

		Genode::size_t   const _ep_stack_size = 4*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 */
		Menu_view_slave(Genode::Pd_session            &pd,
		                Genode::Region_map            &rm,
		                Genode::Ram_session_capability ram,
		                Capability<Nitpicker::Session> nitpicker_session,
		                Capability<Rom_session>        dialog_rom_session,
		                Capability<Report::Session>    hover_report_session,
		                Position                       initial_position)
		:
			_ep(&pd, _ep_stack_size, "nit_fader"),
			_policy(_ep, rm, ram, nitpicker_session, dialog_rom_session,
			        hover_report_session, initial_position),
			_child(rm, _ep, _policy)
		{ }

		void position(Position position) { _policy.position(position); }
};

#endif /* _NIT_FADER_SLAVE_H_ */
