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

namespace Dialog_test {
	using namespace Dialog;
	struct Main;
}


struct Dialog_test::Main : Text_area_widget::Action
{
	Env &_env;
	Heap _heap { _env.ram(), _env.rm() };

	Runtime _runtime { _env, _heap };

	struct Main_dialog : Top_level_dialog
	{
		enum class Payment { CASH, CARD } _payment = Payment::CASH;

		using Payment_button = Select_button<Payment>;

		struct Dishes : Widget<Vbox>
		{
			Id _items[4] { { "Pizza" }, { "Salad" }, { "Pasta" }, { "Soup" } };

			Id selected_item { };

			void view(Scope<Vbox> &s) const
			{
				for (Id const &id : _items) {
					s.sub_scope<Button>(id, [&] (Scope<Vbox, Button> &s) {

						bool const selected = (id == selected_item),
						           hovered  = (s.hovered() && (!s.dragged() || selected));

						if (selected) s.attribute("selected", "yes");
						if (hovered)  s.attribute("hovered",  "yes");

						s.sub_scope<Label>(id.value);
					});
				}
			}

			void click(Clicked_at const &at)
			{
				for (Id const &id : _items)
					if (at.matches<Vbox, Button>(id))
						selected_item = id;
			}
		};

		Allocator &_alloc;
		Text_area_widget::Action &_text_action;
		Hosted<Vbox, Action_button> _inspect { Id { "Inspect" } };
		Hosted<Vbox, Frame, Button, Float, Text_area_widget> _text { Id { "text" }, _alloc };
		Hosted<Vbox, Deferred_action_button> _confirm { Id { "Confirm" } };
		Hosted<Vbox, Deferred_action_button> _cancel  { Id { "Cancel"  } };
		Hosted<Vbox, Hbox, Payment_button>
			_cash { Id { "Cash" }, Payment::CASH },
			_card { Id { "Card" }, Payment::CARD };

		Hosted<Vbox, Frame, Dishes> _dishes { Id { "dishes" } };

		Main_dialog(Name const &name, Allocator &alloc, Text_area_widget::Action &text_action) : Top_level_dialog(name), _alloc(alloc), _text_action(text_action)
		{
			_text.max_lines(1);
			_text.editable(true);
			_text.clear();
			_text.append_newline();
		}

		void view(Scope<> &s) const override
		{
			s.sub_scope<Vbox>([&] (Scope<Vbox> &s) {
				s.sub_scope<Min_ex>(15);

				s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
					s.widget(_dishes); });

				if (_dishes.selected_item.valid()) {
					s.widget(_inspect);
					s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
						s.sub_scope<Button>([&] (Scope<Vbox, Frame, Button> &s) {

							if (s.hovered())
								s.attribute("hovered", "yes");

							s.sub_scope<Float>([&] (Scope<Vbox, Frame, Button, Float> &s) {
								s.attribute("north", "yes");
								s.attribute("east",  "yes");
								s.attribute("west",  "yes");
								s.widget(_text);
							});
						});
					});
					s.sub_scope<Hbox>([&] (Scope<Vbox, Hbox> &s) {
						s.widget(_cash, _payment);
						s.widget(_card, _payment);
					});
					s.widget(_confirm);
					s.widget(_cancel);
				}
			});
		}

		void click(Clicked_at const &at) override
		{
			_dishes .propagate(at);
			_inspect.propagate(at, [&] { log("inspect activated!"); });
			_text   .propagate(at);
			_confirm.propagate(at);
			_cancel .propagate(at);
			_cash   .propagate(at, [&] (Payment p) { _payment = p; });
			_card   .propagate(at, [&] (Payment p) { _payment = p; });
		}

		void clack(Clacked_at const &at) override
		{
			_text.propagate(at, _text_action);
			_confirm.propagate(at, [&] { log("confirm activated!"); });
			_cancel .propagate(at, [&] { _dishes.selected_item = { }; });
		}

		void drag (Dragged_at const &at) override { _text.propagate(at); }

	} _main_dialog { "main", _heap, *this };

	Runtime::View _main_view { _runtime, _main_dialog };

	/* handler used to respond to keyboard input */
	Runtime::Event_handler<Main> _event_handler { _runtime, *this, &Main::_handle_event };

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

		_main_dialog._text.handle_event(event, *this);

//		log("_handle_event: ", event);
	}

	/******************************
	 ** Text_area_widget::Action **
	 ******************************/

	void trigger_copy() override
	{
		log(__func__, " ", __LINE__);
	}

	void trigger_paste() override
	{
		log(__func__, " ", __LINE__);
	}

	void trigger_save() override
	{
		log(__func__, " ", __LINE__);
	}

	void refresh_text_area() override { _main_view.refresh(); }

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	static Dialog_test::Main main(env);
}

