/*
 * \brief  Text dialog
 * \author Norman Feske
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIALOG_H_
#define _DIALOG_H_

#include <passphrase.h>

/* Genode includes */
#include <base/component.h>
#include <base/session_object.h>
#include <os/buffered_xml.h>
#include <os/sandbox.h>
#include <os/dynamic_rom_session.h>
#include <input/event.h>
#include <util/utf8.h>

/* local includes */
#include <dynamic_array.h>
#include <report_session_component.h>
#include <types.h>

namespace Cbe_manager {

	class Dialog;
}

class Cbe_manager::Dialog : private Dynamic_rom_session::Xml_producer
{
	private:

		enum class State
		{
			INIT_TRUST_ANCHOR_SETTINGS,
			INIT_TRUST_ANCHOR_PROGRESS,
			INIT_CBE_SETTINGS,
			INIT_CBE_PROGRESS,
			CBE_CONTROLS
		};

		enum class Init_ta_settings_hover
		{
			NONE,
			PWD,
			OK
		};

		enum class Init_ta_settings_select
		{
			NONE,
			PWD,
			OK
		};

		Dynamic_rom_session     _rom_session;
		State                   _state                   { State::INIT_TRUST_ANCHOR_SETTINGS };
		Passphrase              _init_ta_setg_passphrase { };
		Init_ta_settings_hover  _init_ta_setg_hover      { Init_ta_settings_hover::NONE };
		Init_ta_settings_select _init_ta_setg_select     { Init_ta_settings_hover::PWD };


		/***************************************
		 ** Dynamic_rom_session::Xml_producer **
		 ***************************************/

		void produce_xml(Xml_generator &xml) override;

	public:

		Dialog(Entrypoint    &ep,
		       Ram_allocator &ram,
		       Region_map    &rm);

		void handle_input_event(Input::Event const &);

		void handle_hover(Xml_node const &hover);

		Dynamic_rom_session &rom_session() { return _rom_session; }
};

#endif /* _DIALOG_H_ */
