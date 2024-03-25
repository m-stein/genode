/*
 * \brief  Test for Genode's dialog API
 * \author Norman Feske
 * \date   2023-03-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <dialog/runtime.h>
#include <dialog/widgets.h>
#include <dialog/text_area_widget.h>

using namespace Dialog;

namespace Dialog
{
	struct Small_vgap : Sub_scope
	{
		static void view_sub_scope(auto &s)
		{
			s.node("label", [&] {
				s.attribute("text", "");
				s.attribute("font", "annotation/regular"); });
		}

		static void with_narrowed_at(auto const &, auto const &) { }
	};
}

namespace File_vault_gui { class Main; }

struct File_vault_gui::Main : Text_area_widget::Action
{
	struct Main_dialog : Top_level_dialog
	{
		Allocator &_alloc;
		Text_area_widget::Action &_text_action;
		Hosted<Frame, Vbox, Hbox, Button, Float, Vbox, Text_area_widget> _text { Id { "text" }, _alloc };
		Hosted<Frame, Vbox, Hbox, Action_button> _inspect { Id { "Inspect" } };

		Main_dialog(Name const &name, Allocator &alloc, Text_area_widget::Action &text_action)
		:
			Top_level_dialog(name), _alloc(alloc), _text_action(text_action)
		{
			_text.max_lines(1);
			_text.editable(true);
			_text.clear();
			_text.append_newline();
		}

		void view(Scope<> &s) const override
		{
			s.template sub_scope<Frame>([&] (auto &s) {
				s.template sub_scope<Vbox>([&] (auto &s) {
					s.template sub_scope<Min_ex>(20);
					s.template sub_scope<Hbox>([&] (auto &s) {

						s.template sub_scope<Button>([&] (auto &s) {

							if (s.hovered())
								s.attribute("hovered", "yes");

							s.template sub_scope<Float>([&] (auto &s) {
								s.attribute("west",  "yes");
								s.template sub_scope<Vbox>([&] (auto &s) {
									s.template sub_scope<Min_ex>(20);
									s.widget(_text);
								});
							});
						});
						s.template sub_scope<Small_vgap>();
						s.widget(_inspect);
					});
				});
			});
		}

		void click(Clicked_at const &at) override
		{
			_inspect.propagate(at, [&] { log("inspect activated!"); });
			_text.propagate(at);
		}

		void clack(Clacked_at const &at) override
		{
			_text.propagate(at, _text_action);
		}

		void drag (Dragged_at const &at) override
		{
			_text.propagate(at);
		}
	};

	Env &env;
	Heap heap { env.ram(), env.rm() };
	Runtime runtime { env, heap };
	Main_dialog main_dialog { "main", heap, *this };
	Runtime::View main_view { runtime, main_dialog };
	Runtime::Event_handler<Main> event_handler { runtime, *this, &Main::_handle_event };

	void _handle_event(Dialog::Event const &event)
	{
		enum { CODEPOINT_NEWLINE = 10 };
		bool ignore = false;
		event.event.handle_press([&] (Input::Keycode, Codepoint code) {
			if (code.value == CODEPOINT_NEWLINE)
				ignore = true;
		});
		if (ignore)
			return;

		main_dialog._text.handle_event(event, *this);
	}

	Main(Env &env) : env(env) { }

	/******************************
	 ** Text_area_widget::Action **
	 ******************************/

	void trigger_copy() override { }

	void trigger_paste() override { }

	void trigger_save() override { }

	void refresh_text_area() override { main_view.refresh(); }
};


void Component::construct(Genode::Env &env)
{
	static File_vault_gui::Main main(env);
}

