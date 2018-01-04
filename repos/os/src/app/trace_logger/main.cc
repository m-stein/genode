/*
 * \brief  Log information about present trace subjects
 * \author Norman Feske
 * \date   2015-06-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/session_policy.h>
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>

using namespace Genode;


	template <typename FUNC>
	void for_each_subject(Trace::Subject_id subjects[],
	                      size_t max_subjects, FUNC const &func)
	{
		for (size_t i = 0; i < max_subjects; i++) {
			Trace::Subject_info info = trace.subject_info(subjects[i]);
			func(subjects[i].id, info);
		}
	}


class Trace_buffer_monitor
{
	private:

		enum { MAX_ENTRY_BUF = 256 };

		char                  _buf[MAX_ENTRY_BUF];
		Region_map           &_rm;
		Trace::Subject_id     _id;
		Trace::Buffer        *_buffer;

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';
			return _buf;
		}

	public:

		Trace_buffer_monitor(Region_map           &rm,
		                     Trace::Subject_id     id,
		                     Dataspace_capability  ds_cap)
		:
			_rm(rm), _id(id), _buffer(rm.attach(ds_cap))
		{
			log("monitor "
			    "subject:", _id.id, " "
			    "buffer:",  Hex((addr_t)_buffer));
		}

		~Trace_buffer_monitor()
		{
			if (_buffer) { _rm.detach(_buffer); }
		}

		Trace::Subject_id id() { return _id; };

		void dump()
		{

			Trace::Buffer::Entry _curr_entry = _buffer->first();
			log("overflows: ", _buffer->wrapped());
			log("read all remaining events ", _buffer->_head_offset, " ", _curr_entry.last());


unsigned xxx = 0;
			for (; !_curr_entry.last(); _curr_entry = _buffer->next(_curr_entry)) {
log("entry ", xxx++);
				/* omit empty entries */
				if (_curr_entry.length() == 0) {

					log(".");
					continue;
				}

				char const * const data = _terminate_entry(_curr_entry);
				if (data) { log(data); }
				else {
					log(":");
				}
			}

			/* reset after we read all available entries */
			_curr_entry = _buffer->first();
		}
};

class Trace_subject_registry
{
	private:

		struct Entry : Genode::List<Entry>::Element
		{
			Genode::Trace::Subject_id const id;

			Genode::Trace::Subject_info info;

			/**
			 * Execution time during the last period
			 */
			unsigned long long recent_execution_time = 0;

			Entry(Genode::Trace::Subject_id id) : id(id) { }

			void update(Genode::Trace::Subject_info const &new_info)
			{
				unsigned long long const last_execution_time = info.execution_time().value;
				info = new_info;
				recent_execution_time = info.execution_time().value - last_execution_time;
			}
		};

		Xml_node    _config;
		List<Entry> _entries;

		Entry *_lookup(Genode::Trace::Subject_id const id)
		{
			for (Entry *e = _entries.first(); e; e = e->next())
				if (e->id == id)
					return e;

			return nullptr;
		}

		enum { MAX_SUBJECTS = 512 };
		Genode::Trace::Subject_id _subjects[MAX_SUBJECTS];
		unsigned long _report_cnt { 0 };

		void _sort_by_recent_execution_time()
		{
			Genode::List<Entry> sorted;

			while (_entries.first()) {

				/* find entry with lowest recent execution time */
				Entry *lowest = _entries.first();
				for (Entry *e = _entries.first(); e; e = e->next()) {
					if (e->recent_execution_time < lowest->recent_execution_time)
						lowest = e;
				}

				_entries.remove(lowest);
				sorted.insert(lowest);
			}

			_entries = sorted;
		}


	public:

		Trace_subject_registry(Xml_node config) : _config(config) { }

		void update(Genode::Trace::Connection &trace, Genode::Allocator &alloc)
		{
			unsigned const num_subjects = trace.subjects(_subjects, MAX_SUBJECTS);
			unsigned       num_traced   = num_subjects;

			/* add and update existing entries */
			for (unsigned i = 0; i < num_subjects; i++) {

				Genode::Trace::Subject_id const id = _subjects[i];

				Entry *e = _lookup(id);
				if (!e) {
					e = new (alloc) Entry(id);
					_entries.insert(e);
				}

				e->update(trace.subject_info(id));

				bool label_match = false;
				Session_label const label(e->info.session_label(),
				                          " -> ",
				                          e->info.thread_name().string());
				try {
					Session_policy policy(label, _config);
					label_match = true;
				}
				catch (Session_policy::No_policy_defined) { }

				/* purge dead threads */
				if (e->info.state() == Genode::Trace::Subject_info::DEAD ||
				    !label_match)
				{
					trace.free(e->id);
					_entries.remove(e);
					Genode::destroy(alloc, e);
					log("do not trace \"", label, "\"");
					num_traced--;
				}
			}


			/* enable tracing for test-thread */
			auto enable_tracing = [this, &env] (Trace::Subject_id   id,
			                                    Trace::Subject_info info) {

				try {
					log("enable tracing for "
					    "thread:'", info.thread_name().string(), "' with "
					    "policy:", policy_id.id);

					trace.trace(id.id, policy_id, 16384U);

					Dataspace_capability ds_cap = trace.buffer(id.id);
					test_monitor.construct(env.rm(), id.id, ds_cap);

				} catch (Trace::Source_is_dead) {
					error("source is dead");
					throw -2;
				}
			};


			for_each_subject(subjects, num_subjects, enable_tracing);

			log("trace ", num_traced, " out of ", num_subjects, " subjects");
			_sort_by_recent_execution_time();
		}

		void report(bool report_affinity, bool report_activity)
		{
			log("");
			log("--- Report #", _report_cnt++, " ---");
			for (Entry const *e = _entries.first(); e; e = e->next()) {

				typedef Genode::Trace::Subject_info Subject_info;
				Subject_info::State const state = e->info.state();
				log("<subject label=\"",  e->info.session_label().string(),
				          "\" thread=\"", e->info.thread_name().string(),
				          "\" id=\"",     e->id.id,
				          "\" state=\"",  Subject_info::state_name(state),
				          "\">");

				if (report_activity)
					log("  <activity total=\"",  e->info.execution_time().value,
					             "\" recent=\"", e->recent_execution_time,
					             "\">");

				if (report_affinity)
					log("  <affinity xpos=\"", e->info.affinity().xpos(),
					             "\" ypos=\"", e->info.affinity().ypos(),
					             "\">");

				log("</subject>");
			}
		}
};


namespace App {

	struct Main;
	using namespace Genode;
}


struct App::Main
{
	Env &_env;

	Trace::Connection _trace { _env, 512*1024, 32*1024, 0 };

	static unsigned long _default_period_ms() { return 5000; }

	unsigned long _period_ms = _default_period_ms();

	bool _report_affinity = false;
	bool _report_activity = false;

	Attached_rom_dataspace _config { _env, "config" };

	bool _config_report_attribute_enabled(char const *attr) const
	{
		try {
			return _config.xml().sub_node("report").attribute_value(attr, false);
		} catch (...) { return false; }
	}

	Timer::Connection _timer { _env };

	Heap                   _heap                   { _env.ram(), _env.rm() };
	Trace_subject_registry _trace_subject_registry { _config.xml() };

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	void _handle_period();

	Signal_handler<Main> _periodic_handler = {
		_env.ep(), *this, &Main::_handle_period};

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_periodic_handler);
	}
};


void App::Main::_handle_config()
{
	_config.update();

	_period_ms = _config.xml().attribute_value("period_ms", _default_period_ms());

	_report_affinity = _config_report_attribute_enabled("affinity");
	_report_activity = _config_report_attribute_enabled("activity");

	log("period_ms=",       _period_ms,       ", "
	    "report_activity=", _report_activity, ", "
	    "report_affinity=", _report_affinity);

	_timer.trigger_periodic(1000*_period_ms);
}


void App::Main::_handle_period()
{
	/* update subject information */
	_trace_subject_registry.update(_trace, _heap);

	/* generate report */
	_trace_subject_registry.report(_report_affinity, _report_activity);
}


void Component::construct(Genode::Env &env) { static App::Main main(env); }

