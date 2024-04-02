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
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
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
		using Content_string = String<64>;

		Hosted<Button, Float, Vbox, Text_area_widget> text_area;

		void reset()
		{
			text_area.clear();
			text_area.append_newline();
		}

		One_line_prompt(Allocator &alloc) : text_area(Id { "text_area" }, alloc)
		{
			text_area.max_lines(1);
			text_area.editable(true);
			reset();
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

		template <typename FN>
		void with_content(FN && fn) const
		{
			Constructible<Content_string> content;
			auto write = [&] (char const *str) { content.construct(str); };
			{
				Buffered_output<Content_string::size(), decltype(write)> out(write);
				text_area.for_each_character([&] (Codepoint c) { print(out, c); });
			}
			fn(*content);
		}

		template <typename FN>
		void with_content_as_num_bytes(FN && fn) const
		{
			with_content([&] (auto const &str) {
				Number_of_bytes num_bytes { 0 };
				ascii_to(str.string(), num_bytes);
				fn(num_bytes);
			});
		}

		size_t content_length() const
		{
			size_t size { };
			text_area.for_each_character([&] (Codepoint) { size++; });
			return size;
		}
	};
}

namespace File_vault_gui { class Main; }

struct File_vault_gui::Main : One_line_prompt::Action
{
	using Ui_state_string = String<64>;

	enum Dialog_type { NONE, SETUP, WAIT, CONTROLS, UNLOCK };

	enum { PASSPHRASE_PROMPT_MIN_EX = 20 };
	enum { PASSPHRASE_BUTTON_MIN_EX = 10 };
	enum { MIN_PASSPHRASE_LENGHT = 8 };

	struct Unlock_frame : Widget<Frame>
	{
		Main &main;
		bool show_passphrase = false;
		Hosted<Frame, Vbox, Hbox, One_line_prompt> passphrase_prompt { Id { "passphrase" }, main.heap };
		Hosted<Frame, Vbox, Hbox, Switch_button> show_passphrase_button { Id { "show_passphrase" } };
		Hosted<Frame, Vbox, Action_button> unlock_button { Id { "Unlock" } };

		Unlock_frame(Main &main) : main(main) { }

		bool ready_for_unlock() const
		{
			return passphrase_prompt.content_length() >= MIN_PASSPHRASE_LENGHT;
		}

		void view(Scope<Frame> &s) const
		{
			s.template sub_scope<Vbox>([&] (auto &s) {
				s.template sub_scope<Left_aligned_text>(" Passphrase:");
				s.template sub_scope<Hbox>([&] (auto &s) {
					s.widget(passphrase_prompt, true, PASSPHRASE_PROMPT_MIN_EX);
					s.template sub_scope<Small_vgap>();
					s.widget(show_passphrase_button, show_passphrase, "Hide", "Show", PASSPHRASE_BUTTON_MIN_EX);
				});
				if (ready_for_unlock())
					s.widget(unlock_button);
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase_prompt.propagate(at, [&] { });
			show_passphrase_button.propagate(at, [&] { show_passphrase = !show_passphrase; });
			if (ready_for_unlock())
				unlock_button.propagate(at, [&] { main.unlock(*this); });
		}

		void clack(Clacked_at const &at)
		{
			passphrase_prompt.propagate(at, main);
		}

		void drag(Dragged_at const &at)
		{
			passphrase_prompt.propagate(at);
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (!ready_for_unlock())
						break;

					main.unlock(*this);
					passphrase_prompt.reset();
					break;

				case Input::KEY_TAB: break;
				default: passphrase_prompt.handle_event(event, main); break;
				}
			});
		}
	};

	struct Setup_frame : Widget<Frame>
	{
		enum { PROMPT_MIN_EX = 20 };
		enum { MIN_CAPACITY = 100 * 1024 };

		enum Prompt { PASSPHRASE, CAPACITY, JOURNALING_BUFFER };

		Main &main;
		bool show_passphrase = false;
		Prompt selected_prompt = PASSPHRASE;
		Hosted<Frame, Vbox, Hbox, One_line_prompt> passphrase_prompt { Id { "passphrase" }, main.heap };
		Hosted<Frame, Vbox, Hbox, Switch_button> show_passphrase_button { Id { "show_passphrase" } };
		Hosted<Frame, Vbox, One_line_prompt> capacity_prompt { Id { "capacity" }, main.heap };
		Hosted<Frame, Vbox, One_line_prompt> journal_buf_prompt { Id { "journal_buf" }, main.heap };
		Hosted<Frame, Vbox, Action_button> start_button { Id { "Start" } };

		Setup_frame(Main &main) : main(main) { }

		static size_t min_journal_buf(Number_of_bytes capacity)
		{
			size_t result { (size_t)capacity >> 8 };
			if (result < MIN_CAPACITY)
				result = MIN_CAPACITY;

			return result;
		}

		bool ready_for_setup() const
		{
			if (passphrase_prompt.content_length() < MIN_PASSPHRASE_LENGHT)
				return false;

			bool result = false;
			capacity_prompt.with_content_as_num_bytes([&] (auto capacity) {
				journal_buf_prompt.with_content_as_num_bytes([&] (auto journal_buf) {
					result =
						capacity >= MIN_CAPACITY &&
						journal_buf >= min_journal_buf(capacity);
				});
			});
			return result;
		}

		void view(Scope<Frame> &s) const
		{
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
				s.widget(journal_buf_prompt, selected_prompt == JOURNALING_BUFFER, PROMPT_MIN_EX);
				s.template sub_scope<Left_aligned_text>("");
				s.template sub_scope<Left_aligned_text>(" Image size: 128M");
				s.template sub_scope<Left_aligned_text>("");
				if (ready_for_setup())
					s.widget(start_button);
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase_prompt.propagate(at, [&] { selected_prompt = PASSPHRASE; });
			show_passphrase_button.propagate(at, [&] { show_passphrase = !show_passphrase; });
			capacity_prompt.propagate(at, [&] { selected_prompt = CAPACITY; });
			journal_buf_prompt.propagate(at, [&] { selected_prompt = JOURNALING_BUFFER; });
			if (ready_for_setup())
				start_button.propagate(at, [&] { main.setup(*this); });
		}

		void clack(Clacked_at const &at)
		{
			passphrase_prompt.propagate(at, main);
			capacity_prompt.propagate(at, main);
			journal_buf_prompt.propagate(at, main);
		}

		void drag(Dragged_at const &at)
		{
			passphrase_prompt.propagate(at);
			capacity_prompt.propagate(at);
			journal_buf_prompt.propagate(at);
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
			case JOURNALING_BUFFER: journal_buf_prompt.handle_event(event, main); break;
			}
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (!ready_for_setup())
						break;

					main.setup(*this);
					passphrase_prompt.reset();
					capacity_prompt.reset();
					journal_buf_prompt.reset();
					break;

				case Input::KEY_TAB: select_next_prompt(); break;
				default: forward_to_selected_prompt(event); break;
				}
			});
		}
	};

	struct Wait_frame : Widget<Frame>
	{
		void view(Scope<Frame> &s) const
		{
			s.template sub_scope<Label>(" Please wait ... ");
		}
	};

	struct Controls_frame : Widget<Frame>
	{
		Main &main;
		Hosted<Frame, Vbox, Action_button> dimensions_button { Id { "Dimensions..." } };
		Hosted<Frame, Vbox, Action_button> security_button { Id { "Security..." } };
		Hosted<Frame, Vbox, Action_button> lock_button { Id { "Lock" } };

		Controls_frame(Main &main) : main(main) { }

		void view(Scope<Frame> &s) const
		{
			s.template sub_scope<Vbox>([&] (auto &s) {
				s.template sub_scope<Label>(" Controls ");
				s.widget(dimensions_button);
				s.widget(security_button);
				s.template sub_scope<Label>(" Image: 4,5 MiB ");
				s.template sub_scope<Label>(" Capacity: 1,2 MiB ");
				s.template sub_scope<Label>(" Clients: 12 ");
				s.widget(lock_button);
			});
		}

		void click(Clicked_at const &at)
		{
			lock_button.propagate(at, [&] { main.lock(); });
		}
	};

	struct Main_dialog : Top_level_dialog
	{
		Main &main;

		Main_dialog(Name const &name, Main &main) : Top_level_dialog(name), main(main) { }

		Hosted<Unlock_frame> unlock_frame { Id { "unlock" }, main };
		Hosted<Setup_frame> setup_frame { Id { "setup" }, main };
		Hosted<Wait_frame> wait_frame { Id { "wait" } };
		Hosted<Controls_frame> controls_frame { Id { "controls" }, main };

		void view(Scope<> &s) const override
		{
			switch (main.active_dialog) {
			case UNLOCK: s.widget(unlock_frame); break;
			case SETUP: s.widget(setup_frame); break;
			case WAIT: s.widget(wait_frame); break;
			case CONTROLS: s.widget(controls_frame); break;
			case NONE: s.node("empty", [&] { }); break;
			}
		}

		void click(Clicked_at const &at) override
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.click(at); break;
			case CONTROLS: controls_frame.click(at); break;
			case UNLOCK: unlock_frame.click(at); break;
			default: break;
			}
		}

		void clack(Clacked_at const &at) override
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.clack(at); break;
			case UNLOCK: unlock_frame.clack(at); break;
			default: break;
			}
		}

		void drag(Dragged_at const &at) override
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.drag(at); break;
			case UNLOCK: unlock_frame.drag(at); break;
			default: break;
			}
		}

		void handle_event(Dialog::Event const &event)
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.handle_event(event); break;
			case UNLOCK: unlock_frame.handle_event(event); break;
			default: break;
			}
		}
	};

	Env &env;
	Dialog_type active_dialog { NONE };
	Heap heap { env.ram(), env.rm() };
	Runtime runtime { env, heap };
	Main_dialog main_dialog { "main", *this };
	Runtime::View main_view { runtime, main_dialog };
	Runtime::Event_handler<Main> event_handler { runtime, *this, &Main::handle_event };
	Expanding_reporter ui_config_reporter { env, "ui_config", "ui_config" };
	Attached_rom_dataspace ui_report_rom { env, "ui_report" };
	Signal_handler<Main> signal_handler { env.ep(), *this, &Main::handle_signal };

	void handle_event(Dialog::Event const &event)
	{
		main_dialog.handle_event(event);
	}

	void setup(Setup_frame const &setup_frame)
	{
		ui_config_reporter.generate([&] (Xml_generator &xml) {
			setup_frame.passphrase_prompt.with_content([&] (auto const &str) {
				xml.attribute("passphrase", str);
			});
			setup_frame.capacity_prompt.with_content_as_num_bytes([&] (auto num_bytes) {
				xml.attribute("client_fs_size", num_bytes);
			});
			setup_frame.journal_buf_prompt.with_content_as_num_bytes([&] (auto num_bytes) {
				xml.attribute("journaling_buf_size", num_bytes);
			});
		});
	}

	void unlock(Unlock_frame const &unlock_frame)
	{
		ui_config_reporter.generate([&] (Xml_generator &xml) {
			unlock_frame.passphrase_prompt.with_content([&] (auto const &str) {
				xml.attribute("passphrase", str);
			});
		});
	}

	void lock()
	{
		ui_config_reporter.generate([&] (Xml_generator &) { });
	}

	void handle_signal()
	{
		ui_report_rom.update();
		Xml_node ui_report = ui_report_rom.xml();
		Ui_state_string state = ui_report.attribute_value("state", Ui_state_string());
		Dialog_type dialog_type =
			state == "invalid" ? WAIT :
			state == "uninitialized" ? SETUP :
			state == "initializing" ? WAIT :
			state == "unlocking" ? WAIT :
			state == "unlocked" ? CONTROLS :
			state == "locking" ? WAIT :
			state == "locked" ? UNLOCK :
			NONE;

		if (active_dialog != dialog_type) {
			active_dialog = dialog_type;
			main_view.refresh();
		}
	}

	Main(Env &env) : env(env)
	{
		ui_report_rom.sigh(signal_handler);
		handle_signal();
	}

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

