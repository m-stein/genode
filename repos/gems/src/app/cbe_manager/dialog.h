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
	public:

		Dynamic_rom_session rom_session;

	private:

		enum class Window_type
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

		Allocator &_alloc;

		struct Character : Codepoint
		{
			Character(Codepoint &codepoint) : Codepoint(codepoint) { }

			void print(Output &out) const
			{
				if (value == '"')
					Genode::print(out, "&quot;");
				else if (value == 9)
					Genode::print(out, " ");
				else
					Codepoint::print(out);
			}
		};

		using Line = Dynamic_array<Character>;
		using Text = Dynamic_array<Line>;

		Window_type             _window_type             { Window_type::INIT_TRUST_ANCHOR_SETTINGS };
		Passphrase              _init_ta_setg_passphrase { };
		Init_ta_settings_hover  _init_ta_setg_hover      { Init_ta_settings_hover::NONE };
		Init_ta_settings_select _init_ta_setg_select     { Init_ta_settings_hover::NONE };

		struct Position
		{
			Line::Index x;
			Text::Index y;

			Position(Line::Index x, Text::Index y) : x(x), y(y) { }

			Position(Position const &other) : x(other.x), y(other.y) { }

			bool operator != (Position const &other) const
			{
				return (x.value != other.x.value) || (y.value != other.y.value);
			}
		};

		Position _cursor { { 0 }, { 0 } };

		Position _scroll { { 0 }, { 0 } };

		Constructible<Position> _hovered_position { };

		unsigned _max_lines = ~0U;

		bool _editable = false;

		unsigned _modification_count = 0;

		bool _drag         = false;
		bool _shift        = false;
		bool _control      = false;
		bool _text_hovered = false;

		void produce_xml(Xml_generator &xml) override;

		static bool _printable(Codepoint code)
		{
			if (!code.valid())
				return false;

			if (code.value == '\t')
				return true;

			return (code.value >= 0x20 && code.value < 0xf000);
		}

		void _clamp_scroll_position_to_upper_bound()
		{
		}

		void _move_characters(Line &, Line &);
		void _insert_printable(Codepoint);

	public:

		Dialog(Entrypoint &, Ram_allocator &, Region_map &, Allocator &);

		void editable(bool editable) { _editable = editable; }

		unsigned modification_count() const { return _modification_count; }

		void max_lines(unsigned max_lines) { _max_lines = max_lines; }

		void handle_input_event(Input::Event const &);

		void handle_hover(Xml_node const &hover);

		void clear();

		void append_newline() { }

		void append_character(Codepoint c)
		{
			if (_printable(c)) {
				_init_ta_setg_passphrase.append_character(c);
			}
		}

		/* insert character and advance cursor */
		void insert_at_cursor_position(Codepoint);

		void gen_clipboard_content(Xml_generator &xml) const;
};

#endif /* _DIALOG_H_ */
