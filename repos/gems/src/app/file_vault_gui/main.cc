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

	struct Centered_text : Sub_scope
	{
		static void view_sub_scope(auto &s, auto const &text, unsigned min_ex = 0)
		{
			s.template sub_scope<Vbox>([&] (auto &s) {
				s.template sub_scope<Min_ex>(min_ex);
				s.template sub_scope<Label>(text);
			});
		}

		static void with_narrowed_at(auto const &, auto const &) { }
	};

	struct Left_aligned_text : Sub_scope
	{
		static void view_sub_scope(auto &s, auto const &text)
		{
			s.node("float", [&] {
				s.attribute("west", "yes");
				s.named_sub_node("label", "label", [&] {
					s.attribute("text", text); }); });
		}

		static void with_narrowed_at(auto const &, auto const &) { }
	};

	struct Switch_button : Widget<Button>
	{
		using Text = String<16>;

		template <typename FN>
		void view(Scope<Button> &s, bool selected, FN const &fn) const
		{
			bool const hovered = (s.hovered() && (!s.dragged() || selected));

			if (selected) s.attribute("selected", "yes");
			if (hovered)  s.attribute("hovered",  "yes");

			fn(s);
		}

		void view(Scope<Button> &s, bool on, Text const &on_text, Text const &off_text, unsigned min_ex = 0) const
		{
			view(s, false, [&] (auto &s) {
				s.template sub_scope<Centered_text>(on ? on_text : off_text, min_ex);
			});
		}

		template <typename FN>
		void click(Clicked_at const &, FN const &toggle_fn) const { toggle_fn(); }
	};

	struct One_line_prompt : Widget<Button>
	{
		using Action = Text_area_widget::Action;

		Hosted<Button, Float, Vbox, Text_area_widget> text_area;

		One_line_prompt(Allocator &alloc) : text_area(Id { "text_area" }, alloc)
		{
			text_area.max_lines(1);
			text_area.editable(true);
			text_area.clear();
			text_area.append_newline();
		}

		void view(Scope<Button> &s, bool selected, unsigned min_ex = 0) const
		{
			if (s.hovered()) {
				s.attribute("hovered", "yes");
			}
			if (selected)
				s.attribute("selected", "yes");

			s.template sub_scope<Float>([&] (auto &s) {
				s.attribute("west",  "yes");
				s.template sub_scope<Vbox>([&] (auto &s) {
					s.template sub_scope<Min_ex>(min_ex);
					s.widget(text_area);
				});
			});
		}

		void click(Clicked_at const &at, auto const &fn)
		{
			text_area.propagate(at);
			fn();
		}

		void clack(Clacked_at const &at, Action &action) { text_area.propagate(at, action); }

		void drag (Dragged_at const &at) { text_area.propagate(at); }

		void handle_event(Dialog::Event const &event, Action &action) { text_area.handle_event(event, action); }
	};
}

namespace File_vault_gui { class Main; }

struct File_vault_gui::Main : One_line_prompt::Action
{
	struct Main_dialog : Top_level_dialog
	{
		enum { PASSPHRASE_PROMPT_MIN_EX = 20 };
		enum { PASSPHRASE_BUTTON_MIN_EX = 10 };
		enum { PROMPT_MIN_EX = 20 };
		enum Prompt { PASSPHRASE, CAPACITY, JOURNALING_BUFFER };

		Main &main;
		bool show_passphrase = false;
		Prompt selected_prompt = PASSPHRASE;
		Hosted<Frame, Vbox, Hbox, One_line_prompt> passphrase_prompt { Id { "passphrase" }, main.heap };
		Hosted<Frame, Vbox, Hbox, Switch_button> show_passphrase_button { Id { "show_passphrase" } };
		Hosted<Frame, Vbox, One_line_prompt> capacity_prompt { Id { "capacity" }, main.heap };
		Hosted<Frame, Vbox, One_line_prompt> journaling_buffer_prompt { Id { "journaling_buffer" }, main.heap };
		Hosted<Frame, Vbox, Action_button> start_button { Id { "Start" } };

		Main_dialog(Name const &name, Main &main)
		: Top_level_dialog(name), main(main) { }

		void view(Scope<> &s) const override
		{
			s.template sub_scope<Frame>([&] (auto &s) {
				s.template sub_scope<Vbox>([&] (auto &s) {
					s.template sub_scope<Left_aligned_text>(" Passphrase:");
					s.template sub_scope<Hbox>([&] (auto &s) {
						s.widget(passphrase_prompt, selected_prompt == PASSPHRASE, PASSPHRASE_PROMPT_MIN_EX);
						s.template sub_scope<Small_vgap>();
						s.widget(show_passphrase_button, show_passphrase, "Hide", "Show", PASSPHRASE_BUTTON_MIN_EX);
					});
					s.template sub_scope<Left_aligned_text>("");
					s.template sub_scope<Left_aligned_text>(" Capacity:");
					s.widget(capacity_prompt, selected_prompt == CAPACITY, PROMPT_MIN_EX);
					s.template sub_scope<Left_aligned_text>("");
					s.template sub_scope<Left_aligned_text>(" Journaling buffer:");
					s.widget(journaling_buffer_prompt, selected_prompt == JOURNALING_BUFFER, PROMPT_MIN_EX);
					s.template sub_scope<Left_aligned_text>("");
					s.template sub_scope<Left_aligned_text>(" Image size: 128M");
					s.template sub_scope<Left_aligned_text>("");
					s.widget(start_button);
				});
			});
		}

		void click(Clicked_at const &at) override
		{
			passphrase_prompt.propagate(at, [&] { selected_prompt = PASSPHRASE; });
			show_passphrase_button.propagate(at, [&] { show_passphrase = !show_passphrase; });
			capacity_prompt.propagate(at, [&] { selected_prompt = CAPACITY; });
			journaling_buffer_prompt.propagate(at, [&] { selected_prompt = JOURNALING_BUFFER; });
		}

		void clack(Clacked_at const &at) override
		{
			passphrase_prompt.propagate(at, main);
			capacity_prompt.propagate(at, main);
			journaling_buffer_prompt.propagate(at, main);
		}

		void drag (Dragged_at const &at) override
		{
			passphrase_prompt.propagate(at);
			capacity_prompt.propagate(at);
			journaling_buffer_prompt.propagate(at);
		}

		void select_next_prompt()
		{
			switch (selected_prompt) {
			case PASSPHRASE: selected_prompt = CAPACITY; break;
			case CAPACITY: selected_prompt = JOURNALING_BUFFER; break;
			case JOURNALING_BUFFER: selected_prompt = PASSPHRASE; break;
			}
			main.refresh_text_area();
		}

		void forward_to_selected_prompt(Dialog::Event const &event)
		{
			switch (selected_prompt) {
			case PASSPHRASE: passphrase_prompt.handle_event(event, main); break;
			case CAPACITY: capacity_prompt.handle_event(event, main); break;
			case JOURNALING_BUFFER: journaling_buffer_prompt.handle_event(event, main); break;
			}
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER: break;
				case Input::KEY_TAB: select_next_prompt(); break;
				default: forward_to_selected_prompt(event); break;
				}
			});
		}
	};

	Env &env;
	Heap heap { env.ram(), env.rm() };
	Runtime runtime { env, heap };
	Main_dialog main_dialog { "main", *this };
	Runtime::View main_view { runtime, main_dialog };
	Runtime::Event_handler<Main> event_handler { runtime, *this, &Main::_handle_event };

	void _handle_event(Dialog::Event const &event)
	{
		main_dialog.handle_event(event);
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

