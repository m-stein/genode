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
#include <file_vault/types.h>

using namespace Dialog;
using namespace File_vault;

enum {
	CODEPOINT_CAPITAL_C = 67,
	CODEPOINT_CAPITAL_E = 69,
	CODEPOINT_CAPITAL_J = 74,
	CODEPOINT_CAPITAL_L = 76,
	CODEPOINT_SMALL_C = 99,
	CODEPOINT_SMALL_E = 101,
	CODEPOINT_SMALL_J = 106,
	CODEPOINT_SMALL_L = 108,
};

struct Back_button : Widget<Float>
{
	void view(Scope<Float> &s) const
	{
		s.sub_scope<Button>([&] (Scope<Float, Button> &s) {
			if (s.hovered()) s.attribute("hovered", "yes");
			s.attribute("style", "back");
			s.sub_scope<Hbox>();
		});
	}

	void click(Clicked_at const &, auto const &fn) { fn(); }
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

struct Switch : Widget<Button>
{
	using Text = String<32>;

	bool &on;

	Switch(bool &on) : on(on) { }

	void view(Scope<Button> &s, Text const &on_text, Text const &off_text) const
	{
		bool const hovered = (s.hovered() && (!s.dragged() || on));
		if (hovered) s.attribute("hovered",  "yes");
		s.sub_scope<Label>(on ? on_text : off_text);
	}

	void click(Clicked_at const &) { on = !on; }
};

struct Prompt : Widget<Button>
{
	using Action = Text_area_widget::Action;
	using Text = String<64>;

	Hosted<Button, Float, Vbox, Text_area_widget> text_area;
	bool show_text;

	void reset()
	{
		text_area.clear();
		text_area.append_newline();
	}

	Prompt(Allocator &alloc, bool show_text) : text_area(Id { "text_area" }, alloc), show_text(show_text)
	{
		text_area.max_lines(1);
		text_area.editable(true);
		reset();
	}

	void view(Scope<Button> &s, bool selected) const
	{
		static constexpr char bullet_utf8[4] { (char)0xe2, (char)0x80, (char)0xa2, 0 };
		if (s.hovered())
			s.attribute("hovered", "yes");
		if (selected)
			s.attribute("selected", "yes");

		s.sub_scope<Float>([&] (Scope<Button, Float> &s) {
			s.attribute("west",  "yes");
			s.sub_scope<Vbox>([&] (Scope<Button, Float, Vbox> &s) {
				s.sub_scope<Min_ex>(20);
				if (show_text)
					s.widget(text_area);
				else {
					Constructible<Text> text;
					auto write = [&] (char const *str) { text.construct(str); };
					{
						Buffered_output<Text::size(), decltype(write)> out(write);
						text_area.for_each_character([&] (Codepoint) { print(out, bullet_utf8); });
					}
					s.sub_scope<Left_aligned_text>(*text);
				}
			});
		});
	}

	void click(Clicked_at const &, auto const &fn) { fn(); }

	void handle_event(Dialog::Event const &event, Action &action) { text_area.handle_event(event, action); }

	void with_text(auto const &fn) const
	{
		Constructible<Text> text;
		auto write = [&] (char const *str) { text.construct(str); };
		{
			Buffered_output<Text::size(), decltype(write)> out(write);
			text_area.for_each_character([&] (Codepoint c) { print(out, c); });
		}
		fn(*text);
	}

	Number_of_bytes as_num_bytes() const
	{
		Number_of_bytes result { };
		with_text([&] (auto const &str) {
			ascii_to(str.string(), result); });
		return result;
	}

	size_t text_length() const
	{
		size_t size { };
		text_area.for_each_character([&] (Codepoint) { size++; });
		return size + 1;
	}
};

struct Main : Prompt::Action
{
	using Ui_state_string = String<64>;

	enum Dialog_type { NONE, SETUP, WAIT, CONTROLS, UNLOCK };

	struct Unlock_frame : Widget<Frame>
	{
		Main &main;
		Hosted<Frame, Vbox, Hbox, Prompt> passphrase { Id { "Passphrase" }, main.heap, false };
		Hosted<Frame, Vbox, Hbox, Switch> show_passphrase { Id { "Show Passphrase" }, passphrase.show_text };
		Hosted<Frame, Vbox, Action_button> unlock_button { Id { "Unlock" } };

		Unlock_frame(Main &main) : main(main) { }

		bool passphrase_long_enough() const { return passphrase.text_length() >= MIN_PASSPHRASE_LENGTH + 1; }

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				s.sub_scope<Left_aligned_text>(" Passphrase:");
				s.sub_scope<Hbox>([&] (Scope<Frame, Vbox, Hbox> &s) {
					s.widget(passphrase, true);
					s.widget(show_passphrase, "Hide", "Show");
				});
				if (passphrase_long_enough())
					s.widget(unlock_button);
				else
					s.sub_scope<Left_aligned_text>(String<64>(" Minimum length: ", (size_t)MIN_PASSPHRASE_LENGTH));
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase.propagate(at, [&] { });
			show_passphrase.propagate(at);
			unlock_button.propagate(at, [&] { unlock(); });
		}

		void unlock()
		{
			main.unlock(*this);
			passphrase.reset();
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (passphrase_long_enough())
						unlock();
					break;

				case Input::KEY_TAB: break;
				default: passphrase.handle_event(event, main); break;
				}
			});
		}
	};

	struct Setup_frame : Widget<Frame>
	{
		enum { MIN_CAPACITY = 100 * 1024 };

		enum Prompt_type { PASSPHRASE, CAPACITY, JOURNALING_BUFFER };

		Main &main;
		Prompt_type selected = PASSPHRASE;
		Hosted<Frame, Vbox, Hbox, Prompt> passphrase { Id { "Passphrase" }, main.heap, false };
		Hosted<Frame, Vbox, Hbox, Switch> show_passphrase { Id { "Show Passphrase" }, passphrase.show_text };
		Hosted<Frame, Vbox, Prompt> capacity { Id { "Capacity" }, main.heap, true };
		Hosted<Frame, Vbox, Prompt> journal_buf { Id { "Journaling Buffer" }, main.heap, true };
		Hosted<Frame, Vbox, Action_button> start_button { Id { "Start" } };

		Setup_frame(Main &main) : main(main) { }

		size_t min_journal_buf() const
		{
			size_t result { (size_t)capacity.as_num_bytes() >> 8 };
			if (result < MIN_CAPACITY)
				result = MIN_CAPACITY;

			return result;
		}

		bool passphrase_long_enough() const { return passphrase.text_length() >= MIN_PASSPHRASE_LENGTH + 1; }

		bool capacity_sufficient() const { return capacity.as_num_bytes() >= MIN_CAPACITY; }

		bool journal_buf_sufficient() const { return journal_buf.as_num_bytes() >= min_journal_buf(); }

		bool ready_to_setup() const { return passphrase_long_enough() && capacity_sufficient() && journal_buf_sufficient(); }

		Number_of_bytes image_size() const
		{
			return
				TRESOR_BLOCK_SIZE *
				tresor_nr_of_blocks(
					TRESOR_NR_OF_SUPERBLOCKS, TRESOR_VBD_MAX_LVL + 1, TRESOR_VBD_DEGREE,
					tresor_tree_num_leaves(capacity.as_num_bytes()), TRESOR_FREE_TREE_MAX_LVL + 1,
					TRESOR_FREE_TREE_DEGREE, tresor_tree_num_leaves(journal_buf.as_num_bytes()));
		}

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				s.sub_scope<Left_aligned_text>(" Passphrase:");
				s.sub_scope<Hbox>([&] (Scope<Frame, Vbox, Hbox> &s) {
					s.widget(passphrase, selected == PASSPHRASE);
					s.widget(show_passphrase, "Hide", "Show");
				});
				if (!passphrase_long_enough())
					s.sub_scope<Left_aligned_text>(String<64>(" Minimum length: ", (size_t)MIN_PASSPHRASE_LENGTH));

				s.sub_scope<Left_aligned_text>("");
				s.sub_scope<Left_aligned_text>(" Capacity:");
				s.widget(capacity, selected == CAPACITY);
				if (!capacity_sufficient())
					s.sub_scope<Left_aligned_text>(String<64>(" Minimum: ", Number_of_bytes(MIN_CAPACITY)));

				s.sub_scope<Left_aligned_text>("");
				s.sub_scope<Left_aligned_text>(" Journaling buffer:");
				s.widget(journal_buf, selected == JOURNALING_BUFFER);
				if (!journal_buf_sufficient())
					s.sub_scope<Left_aligned_text>(String<64>(" Minimum: ", min_journal_buf()));

				if (capacity_sufficient() && journal_buf_sufficient()) {
					s.sub_scope<Left_aligned_text>("");
					s.sub_scope<Left_aligned_text>(String<64>(" Image size: ", image_size()));
				}
				if (ready_to_setup()) {
					s.sub_scope<Left_aligned_text>("");
					s.widget(start_button);
				}
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase.propagate(at, [&] { selected = PASSPHRASE; });
			show_passphrase.propagate(at);
			capacity.propagate(at, [&] { selected = CAPACITY; });
			journal_buf.propagate(at, [&] { selected = JOURNALING_BUFFER; });
			start_button.propagate(at, [&] { setup(); });
		}

		void select_next()
		{
			switch (selected) {
			case PASSPHRASE: selected = CAPACITY; break;
			case CAPACITY: selected = JOURNALING_BUFFER; break;
			case JOURNALING_BUFFER: selected = PASSPHRASE; break;
			}
			main.main_view.refresh();
		}

		void forward_to_selected(Dialog::Event const &event)
		{
			switch (selected) {
			case PASSPHRASE: passphrase.handle_event(event, main); break;
			case CAPACITY: capacity.handle_event(event, main); break;
			case JOURNALING_BUFFER: journal_buf.handle_event(event, main); break;
			}
		}

		void setup()
		{
			main.setup(*this);
			passphrase.reset();
			capacity.reset();
			journal_buf.reset();
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (ready_to_setup())
						setup();
					break;

				case Input::KEY_TAB: select_next(); break;
				default: forward_to_selected(event); break;
				}
			});
		}
	};

	struct Wait_frame : Widget<Frame>
	{
		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Label>(" Please wait ... ");
		}
	};

	struct Controls_frame : Widget<Frame>
	{
		enum Tab { HOME, ENCRYPTION_KEY, CAPACITY, JOURNALING_BUFFER };

		struct Navigation_bar : Widget<Float>
		{
			Controls_frame &controls;
			Hosted<Float, Hbox, Back_button> back_button { Id { "Back" } };

			Navigation_bar(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Float> &s, String<32> const &text) const
			{
				s.attribute("west", "yes");
				s.sub_scope<Hbox>([&] (Scope<Float, Hbox> &s) {
					s.widget(back_button);
					s.node("float", [&] {
						s.attribute("west", "yes");
						s.named_sub_node("label", "label", [&] {
							s.attribute("font", "title/regular");
							s.attribute("text", text); }); }); });
			}

			void click(Clicked_at const &at) { back_button.propagate(at, [&] { controls.visible_tab = HOME; }); }
		};

		struct Home : Widget<Vbox>
		{
			Controls_frame &controls;
			Hosted<Vbox, Action_button> capacity_button { Id { "Capacity" } };
			Hosted<Vbox, Action_button> journal_buf_button { Id { "Journaling Buffer" } };
			Hosted<Vbox, Action_button> encrypt_key_button { Id { "Encryption Key" } };

			Home(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Vbox> &s) const
			{
				if (!controls.main.num_clients.value)
					s.widget(capacity_button);

				s.widget(journal_buf_button);
				s.widget(encrypt_key_button);

				if (controls.main.num_clients.value) {
					s.sub_scope<Left_aligned_text>("");
					s.sub_scope<Left_aligned_text>(" Capacity unchangeable when in use!");
				}
			}

			void click(Clicked_at const &at)
			{
				capacity_button.propagate(at, [&] { controls.visible_tab = CAPACITY; });
				journal_buf_button.propagate(at, [&] { controls.visible_tab = JOURNALING_BUFFER; });
				encrypt_key_button.propagate(at, [&] { controls.visible_tab = ENCRYPTION_KEY; });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode, Codepoint code) {
					switch (code.value) {
					case CODEPOINT_CAPITAL_C:
					case CODEPOINT_SMALL_C: controls.switch_to_tab(CAPACITY); break;
					case CODEPOINT_CAPITAL_J:
					case CODEPOINT_SMALL_J: controls.switch_to_tab(JOURNALING_BUFFER); break;
					case CODEPOINT_CAPITAL_E:
					case CODEPOINT_SMALL_E: controls.switch_to_tab(ENCRYPTION_KEY); break;
					default: break;
					}
				});
			}
		};

		template <Ui_config::Extend::Tree TREE>
		struct Dimension_tab : Widget<Vbox>
		{
			enum { MIN_NUM_BYTES = 4096 };

			using Title = String<32>;

			Controls_frame &controls;
			Hosted<Vbox, Navigation_bar> navigation_bar { Id { "Navigation Bar" }, controls };
			Hosted<Vbox, Prompt> num_bytes_prompt { Id { "Number Of Bytes" }, controls.main.heap, true };
			Hosted<Vbox, Action_button> extend_button { Id { "Extend" } };

			Dimension_tab(Controls_frame &controls) : controls(controls) { }

			bool num_bytes_sufficient() const
			{
				bool result = false;
				
					if (num_bytes_prompt.as_num_bytes() >= MIN_NUM_BYTES)
						result = true;

				return result;
			}

			void view(Scope<Vbox> &s) const
			{
				s.widget(navigation_bar,
					TREE == Ui_config::Extend::VIRTUAL_BLOCK_DEVICE ? "Capacity " :
					TREE == Ui_config::Extend::FREE_TREE ? "Journaling Buffer " : "?");

				if (controls.main.ready_to_extend()) {
					s.widget(num_bytes_prompt, true);
					if (num_bytes_sufficient())
						s.widget(extend_button);
					else
						s.sub_scope<Left_aligned_text>(String<64>(" Minimum: ", Number_of_bytes(MIN_NUM_BYTES)));
				} else
					s.sub_scope<Left_aligned_text>(" Please wait ... ");
			}

			void extend()
			{
				controls.main.extend<TREE>(*this);
				num_bytes_prompt.reset();
			}

			void click(Clicked_at const &at)
			{
				navigation_bar.propagate(at);
				extend_button.propagate(at, [&] { extend(); });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode key, Codepoint) {
					switch (key) {
					case Input::KEY_ENTER:

						if (controls.main.ready_to_extend() && num_bytes_sufficient())
							extend();
						break;

					case Input::KEY_ESC: controls.switch_to_tab(HOME); break;
					case Input::KEY_TAB: break;
					default:

						if (controls.main.ready_to_extend())
							num_bytes_prompt.handle_event(event, controls.main);
						break;
					}
				});
			}
		};

		struct Encryption_key : Widget<Vbox>
		{
			Controls_frame &controls;
			Hosted<Vbox, Navigation_bar> navigation_bar { Id { "Navigation Bar" }, controls };
			Hosted<Vbox, Action_button> replace_button { Id { "Replace" } };

			Encryption_key(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Vbox> &s) const
			{
				s.widget(navigation_bar, "Encryption Key ");
				if (controls.main.ready_to_rekey())
					s.widget(replace_button);
				else
					s.sub_scope<Left_aligned_text>(" Please wait ... ");
			}

			void click(Clicked_at const &at)
			{
				navigation_bar.propagate(at);
				replace_button.propagate(at, [&] { controls.main.rekey(); });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode key, Codepoint) {
					switch (key) {
					case Input::KEY_ENTER:

						if (controls.main.ready_to_rekey())
							controls.main.rekey();
						break;

					case Input::KEY_ESC: controls.switch_to_tab(HOME); break;
					default: break;
					}
				});
			}
		};

		Main &main;
		Tab visible_tab { HOME };
		Hosted<Frame, Vbox, Home> home { Id { "Home" }, *this };
		Hosted<Frame, Vbox, Dimension_tab<Ui_config::Extend::VIRTUAL_BLOCK_DEVICE> > capacity { Id { "Capacity" }, *this };
		Hosted<Frame, Vbox, Dimension_tab<Ui_config::Extend::FREE_TREE> > journal_buf { Id { "Journaling Buffer" }, *this };
		Hosted<Frame, Vbox, Encryption_key> encryption_key { Id { "Encryption Key" }, *this };
		Hosted<Frame, Vbox, Action_button> lock_button { Id { "Lock" } };

		Controls_frame(Main &main) : main(main) { }

		void switch_to_tab(Tab tab)
		{
			visible_tab = tab;
			main.main_view.refresh();
		}

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				switch (visible_tab) {
				case HOME: s.widget(home); break;
				case ENCRYPTION_KEY: s.widget(encryption_key); break;
				case CAPACITY: s.widget(capacity); break;
				case JOURNALING_BUFFER: s.widget(journal_buf); break;
				}
				s.sub_scope<Left_aligned_text>("");
				s.sub_scope<Left_aligned_text>(String<32>(" Image: ", main.image_size));
				s.sub_scope<Left_aligned_text>(String<32>(" Capacity: ", main.capacity));
				s.sub_scope<Left_aligned_text>(String<32>(" Clients: ", main.num_clients.value));
				s.sub_scope<Left_aligned_text>("");
				s.widget(lock_button);
			});
		}

		void lock()
		{
			main.lock();
			visible_tab = HOME;
			capacity.num_bytes_prompt.reset();
			journal_buf.num_bytes_prompt.reset();
		}

		void click(Clicked_at const &at)
		{
			switch (visible_tab) {
			case HOME: home.propagate(at); break;
			case ENCRYPTION_KEY: encryption_key.propagate(at); break;
			case CAPACITY: capacity.propagate(at); break;
			case JOURNALING_BUFFER: journal_buf.propagate(at); break;
			default: break;
			}
			lock_button.propagate(at, [&] { lock(); });
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode, Codepoint code) {
				switch (code.value) {
				case CODEPOINT_CAPITAL_L:
				case CODEPOINT_SMALL_L: lock(); break;
				default:

					switch (visible_tab) {
					case HOME: home.handle_event(event); break;
					case CAPACITY: capacity.handle_event(event); break;
					case JOURNALING_BUFFER: journal_buf.handle_event(event); break;
					case ENCRYPTION_KEY: encryption_key.handle_event(event); break;
					}
					break;
				}
			});
		}

		void handle_signal()
		{
			if (visible_tab == CAPACITY && main.num_clients.value)
				visible_tab = HOME;
		}
	};

	struct Main_dialog : Top_level_dialog
	{
		Main &main;
		Hosted<Unlock_frame> unlock_frame { Id { "unlock" }, main };
		Hosted<Setup_frame> setup_frame { Id { "setup" }, main };
		Hosted<Wait_frame> wait_frame { Id { "wait" } };
		Hosted<Controls_frame> controls_frame { Id { "controls" }, main };

		Main_dialog(Name const &name, Main &main) : Top_level_dialog(name), main(main) { }

		void handle_event(Dialog::Event const &event)
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.handle_event(event); break;
			case UNLOCK: unlock_frame.handle_event(event); break;
			case CONTROLS: controls_frame.handle_event(event); break;
			default: break;
			}
		}

		void handle_signal()
		{
			if (main.active_dialog == CONTROLS)
				controls_frame.handle_signal();
		}

		/**********************
		 ** Top_level_dialog **
		 **********************/

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
	};

	Env &env;
	Dialog_type active_dialog { NONE };
	Heap heap { env.ram(), env.rm() };
	Runtime runtime { env, heap };
	Main_dialog main_dialog { "main", *this };
	Runtime::View main_view { runtime, main_dialog };
	Runtime::Event_handler<Main> event_handler { runtime, *this, &Main::handle_event };
	Ui_config ui_config { };
	Expanding_reporter ui_config_reporter { env, "ui_config", "ui_config" };
	Attached_rom_dataspace ui_report_rom { env, "ui_report" };
	Signal_handler<Main> signal_handler { env.ep(), *this, &Main::handle_signal };
	Constructible<Rekey_report> rekey_report { };
	Constructible<Extend_report> extend_report { };
	Number_of_bytes image_size { };
	Number_of_bytes capacity { };
	Number_of_clients num_clients { };

	void handle_event(Dialog::Event const &event)
	{
		main_dialog.handle_event(event);
	}

	void setup(Setup_frame const &setup_frame)
	{
		ui_config.client_fs_size = setup_frame.capacity.as_num_bytes();
		ui_config.journaling_buf_size = setup_frame.journal_buf.as_num_bytes();
		setup_frame.passphrase.with_text([&] (auto const &str) { ui_config.passphrase = str; });
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void unlock(Unlock_frame const &unlock_frame)
	{
		unlock_frame.passphrase.with_text([&] (auto const &str) { ui_config.passphrase = str; });
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void lock()
	{
		ui_config.passphrase = Passphrase();
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	bool ready_to_extend() const
	{
		if (!ui_config.extend.constructed())
			return true;

		if (!extend_report.constructed())
			return false;

		return extend_report->id.value == ui_config.extend->id.value && extend_report->finished;
	}

	bool ready_to_rekey() const
	{
		if (!ui_config.rekey.constructed())
			return true;

		if (!rekey_report.constructed())
			return false;

		return rekey_report->id.value == ui_config.rekey->id.value && rekey_report->finished;
	}

	void rekey()
	{
		Operation_id id { rekey_report.constructed() ? rekey_report->id.value + 1 : 0 };
		ui_config.rekey.construct(id);
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	template <Ui_config::Extend::Tree TREE>
	void extend(Controls_frame::Dimension_tab<TREE> const &dimension_tab)
	{
		Operation_id id { extend_report.constructed() ? extend_report->id.value + 1 : 0 };
		ui_config.extend.construct(id, TREE, dimension_tab.num_bytes_prompt.as_num_bytes());
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void handle_signal()
	{
		ui_report_rom.update();
		Xml_node ui_report = ui_report_rom.xml();
		Ui_state_string state = ui_report.attribute_value("state", Ui_state_string());
		active_dialog =
			state == "invalid" ? WAIT :
			state == "uninitialized" ? SETUP :
			state == "initializing" ? WAIT :
			state == "unlocking" ? WAIT :
			state == "unlocked" ? CONTROLS :
			state == "locking" ? WAIT :
			state == "locked" ? UNLOCK : NONE;

		image_size = ui_report.attribute_value("image_size", Number_of_bytes());
		capacity = ui_report.attribute_value("capacity", Number_of_bytes());
		num_clients.value = ui_report.attribute_value("num_clients", 0ULL);
		ui_report.with_optional_sub_node("rekey", [&] (Xml_node const &rekey) {
			rekey_report.construct(rekey); });

		ui_report.with_optional_sub_node("extend", [&] (Xml_node const &extend) {
			extend_report.construct(extend); });

		main_dialog.handle_signal();
		main_view.refresh();
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


void Component::construct(Genode::Env &env) { static Main main(env); }
