/*
 * \brief  Scripted test for the CBE component
 * \author Norman Feske
 * \author Martin Stein
 * \date   2017-02-16
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/component.h>
#include <base/session_label.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <log_session/log_session.h>
#include <root/component.h>
#include <os/reporter.h>
#include <base/sleep.h>

namespace Test {

	struct Log_message_handler;
	class  Log_session_component;
	class  Log_root;
	struct Main;

	using namespace Genode;

	static bool xml_matches(Xml_node, Xml_node);
}


/***************
 ** Utilities **
 ***************/

static bool Test::xml_matches(Xml_node exp_parent, Xml_node got_parent)
{
	/* compare all children of the parent node */
	try {
		for (unsigned child_idx = 0; child_idx < ~(unsigned)0; child_idx++) {
			Xml_node const exp_child { exp_parent.sub_node(child_idx) };
			try {
				/* compare child type */
				Xml_node const got_child { got_parent.sub_node(child_idx) };
				if (exp_child.type() != got_child.type()) {
					return false;
				}
				/* compare all child attributes */
				try {
					for (unsigned child_attr_idx = 0; child_attr_idx < ~(unsigned)0; child_attr_idx++) {
						Xml_attribute const exp_child_attr { exp_child.attribute(child_attr_idx) };
						struct Mismatch : Genode::Exception { };
						try {
							/* compare attribute name */
							Xml_attribute const got_child_attr { got_child.attribute(child_attr_idx) };
							if (exp_child_attr.name() != got_child_attr.name()) {
								return false;
							}
							/* compare attribute value */
							exp_child_attr.with_raw_value([&] (char const *exp_val, size_t exp_val_size) {
								got_child_attr.with_raw_value([&] (char const *got_val, size_t got_val_size) {
									if (exp_val_size != got_val_size) {
										throw Mismatch();
									}
									if (memcmp(exp_val, got_val, exp_val_size)) {
										throw Mismatch();
									}
								});
							});
						}
						catch (Xml_node::Nonexistent_attribute) { return false; }
						catch (Mismatch)                        { return false; }
					}
				}
				catch (Xml_node::Nonexistent_attribute) { }

				/* compare child content */
				if (!xml_matches(exp_child, got_child)) {
					return false; }
			}
			catch (Xml_node::Nonexistent_sub_node) { return false; }
		}
	}
	catch (Xml_node::Nonexistent_sub_node) { return true; }
	return false;
}


struct Test::Log_message_handler : Interface
{
	typedef String<Log_session::MAX_STRING_LEN> Message;

	virtual void handle_log_message(Message const &message) = 0;
};


class Test::Log_session_component : public Rpc_object<Log_session>
{
	private:

		Session_label  const _label;
		Log_message_handler &_handler;

	public:

		Log_session_component(Session_label const &label, Log_message_handler &handler)
		:
			_label(label), _handler(handler)
		{ }

		size_t write(String const &string) override
		{
			/* strip known line delimiter from incoming message */
			unsigned n = 0;
			static char                          const delim[] { "\033[0m\n" };
			static Genode::String<sizeof(delim)> const pattern { delim };
			for (char const *s = string.string(); s[n] && pattern != s + n; n++);

			Log_message_handler::Message const
				message("[", _label, "] ", Cstring(string.string(), n));

			_handler.handle_log_message(message);
			log(message);

			return strlen(string.string());
		}
};


class Test::Log_root : public Root_component<Log_session_component>
{
	private:

		Log_message_handler &_handler;

	public:

		Log_root(Entrypoint &ep, Allocator &md_alloc, Log_message_handler &handler)
		:
			Root_component(ep, md_alloc), _handler(handler)
		{ }

		Log_session_component *_create_session(const char *args, Affinity const &) override
		{
			Session_label const label = label_from_args(args);

			return new (md_alloc()) Log_session_component(label, _handler);
		}
};


struct Test::Main : Log_message_handler
{
	Env                          &_env;
	Timer::Connection             _timer                { _env };
	bool                          _timer_scheduled      { false };
	Expanding_reporter            _init_config_reporter { _env, "config", "init.config" };
	Attached_rom_dataspace        _config               { _env, "config" };
	size_t            const       _num_steps            { _config.xml().num_sub_nodes() };
	unsigned                      _curr_step            { 0 };
	Attached_rom_dataspace        _block_state          { _env, "block_state" };
	Signal_handler<Main>          _block_state_handler  { _env.ep(), *this, &Main::_handle_block_state };
	Signal_handler<Main>          _timer_handler        { _env.ep(), *this, &Main::_handle_timer };
	Sliced_heap                   _sliced_heap          { _env.ram(), _env.rm() };
	Log_root                      _log_root             { _env.ep(), _sliced_heap, *this };
	bool                          _expect_log           { false };
	Log_message_handler::Message  _expect_log_msg       { };

	void _publish_report(Expanding_reporter &reporter, Xml_node node)
	{
		typedef String<64> Version;
		Version const version = node.attribute_value("version", Version());

		reporter.generate([&] (Genode::Xml_generator &xml) {

			xml.attribute("verbose", "yes");

			if (version.valid())
				xml.attribute("version", version);

			node.with_raw_content([&] (char const *start, size_t length) {
				xml.append(start, length); });
		});
	}

	Xml_node _curr_step_xml() const { return _config.xml().sub_node(_curr_step); }

	void _handle_block_state()
	{
		_block_state.update();
		_execute_curr_step();
	}

	void _advance_step()
	{
		_curr_step++;

		/* exit when reaching the end of the sequence */
		if (_curr_step == _num_steps) {
			_env.parent().exit(0);
			sleep_forever();
		}
	};

	void _execute_curr_step()
	{
		for (;;) {
			Xml_node const step = _curr_step_xml();

			log("step ", _curr_step, " (", step.type(), ")");

			if (step.type() == "expect_log") {
				_expect_log     = true;
				_expect_log_msg = _curr_step_xml().attribute_value("string", Log_message_handler::Message());
				return;
			}

			if (step.type() == "expect_block_state") {
				if (xml_matches(step, _block_state.xml())) {
					_advance_step();
					continue;
				} else {
					warning("init state does not match: ", _block_state.xml());
					warning("expected condition: ", step);
				}
				return;
			}

			if (step.type() == "init_config") {
				_publish_report(_init_config_reporter, step);
				_advance_step();
				continue;
			}

			if (step.type() == "message") {
				typedef String<80> Message;
				Message const message = step.attribute_value("string", Message());
				log("\n--- ", message, " ---");
				_advance_step();
				continue;
			}

			if (step.type() == "nop") {
				_advance_step();
				continue;
			}

			if (step.type() == "sleep") {
				if (!_timer_scheduled) {
					uint64_t const timeout_ms = step.attribute_value("ms", (uint64_t)250);
					_timer.trigger_once(timeout_ms*1000);
					_timer_scheduled = true;
				}
				return;
			}

			error("unexpected step: ", step);
			throw Exception();
		}
	}

	/**
	 * Log_message_handler interface
	 */
	void handle_log_message(Log_message_handler::Message const &message) override
	{
		if (!_expect_log)
			return;

		if (_expect_log_msg != message)
			return;

		_expect_log = false;
		_advance_step();
		_execute_curr_step();
	}

	/*
	 * Timer handling
	 */
	void _handle_timer()
	{
		if (_curr_step_xml().type() != "sleep") {
			error("got spurious timeout signal");
			throw Exception();
		}

		_timer_scheduled = false;

		_advance_step();
		_execute_curr_step();
	}

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_block_state.sigh(_block_state_handler);
		_execute_curr_step();
		_env.parent().announce(_env.ep().manage(_log_root));
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

