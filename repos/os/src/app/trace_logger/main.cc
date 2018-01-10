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


class Buffer_monitor : public List<Buffer_monitor>::Element
{
	private:

		enum { MAX_ENTRY_BUF = 256 };

		char                  _buf[MAX_ENTRY_BUF];
		Region_map           &_rm;
		Trace::Subject_id     _id;
		Trace::Buffer        &_buffer;

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, (unsigned)MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';
			return _buf;
		}

	public:

		Buffer_monitor(Region_map           &rm,
		               Trace::Subject_id     id,
		               Dataspace_capability  ds_cap)
		:
			_rm(rm), _id(id),
			_buffer(*(Trace::Buffer *)rm.attach(ds_cap))
		{
			log("monitor "
			    "subject:", _id.id, " "
			    "buffer:",  &_buffer);
		}

		~Buffer_monitor()
		{
			_buffer.~Buffer();
			_rm.detach(&_buffer);
		}

		Trace::Subject_id id() { return _id; };

		void dump()
		{
			Trace::Buffer::Entry _curr_entry = _buffer.first();
			log("overflows: ", _buffer.wrapped());
			log("read all remaining events ", _buffer._head_offset, " ", _curr_entry.last());

unsigned xxx = 0;
			for (; !_curr_entry.last(); _curr_entry = _buffer.next(_curr_entry)) {
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
			_curr_entry = _buffer.first();
		}
};


class Main
{
	private:

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

		struct Entry : List<Entry>::Element
		{
			Trace::Subject_id const id;
			Trace::Subject_info     info                  { };
			unsigned long long      recent_execution_time { 0 };

			Entry(Trace::Subject_id id) : id(id) { }

			void update(Trace::Subject_info const &new_info)
			{
				unsigned long long const last_execution_time =
					info.execution_time().value;

				info = new_info;
				recent_execution_time =
					info.execution_time().value - last_execution_time;
			}
		};

		Env                      &_env;
		Trace::Connection         _trace            { _env, 1024*1024*10, 64*1024, 0 };
		unsigned long             _period_ms        { DEFAULT_PERIOD_MS };
		bool                      _report_affinity  { false };
		bool                      _report_activity  { false };
		Attached_rom_dataspace    _config_rom       { _env, "config" };
		Xml_node                  _config           { _config_rom.xml() };
		Timer::Connection         _timer            { _env };
		Heap                      _heap             { _env.ram(), _env.rm() };
		List<Entry>               _entries          { };
		List<Buffer_monitor>      _monitors         { };
		String<32>                _policy_name      { "rpc_name" };
		Rom_connection            _policy_rom       { _env, _policy_name.string() };
		Rom_dataspace_capability  _policy_ds        { _policy_rom.dataspace() };
		Session_label             _policy_label     { "init -> test-trace_logger" };
		size_t                    _policy_size      { Dataspace_client(_policy_ds).size() };
		Trace::Policy_id          _policy_id        { _trace.alloc_policy(_policy_size) };
		unsigned long             _report_cnt       { 0 };
		Trace::Subject_id         _subjects[MAX_SUBJECTS];
		Trace::Subject_id         _traced_subjects[MAX_SUBJECTS];
		Signal_handler<Main>      _config_handler   { _env.ep(), *this, &Main::_handle_config};
		Signal_handler<Main>      _periodic_handler { _env.ep(), *this, &Main::_handle_period};

		Entry *_lookup_entry(Trace::Subject_id const id)
		{
			for (Entry *e = _entries.first(); e; e = e->next())
				if (e->id == id)
					return e;
			return nullptr;
		}

		bool _config_report_attribute_enabled(char const *attr) const
		{
			try {
				return _config.sub_node("report").attribute_value(attr, false);
			} catch (...) { return false; }
		}

		void _sort_by_recent_execution_time()
		{
//			List<Entry> sorted;
//
//			while (_entries.first()) {
//
//				/* find entry with lowest recent execution time */
//				Entry *lowest = _entries.first();
//				for (Entry *e = _entries.first(); e; e = e->next()) {
//					if (e->recent_execution_time < lowest->recent_execution_time)
//						lowest = e;
//				}
//
//				_entries.remove(lowest);
//				sorted.insert(lowest);
//			}
//
//			_entries = sorted;
		}

		void _handle_config()
		{
			_config_rom.update();

			_period_ms = _config.attribute_value("period_ms", (unsigned long)DEFAULT_PERIOD_MS);

			_report_affinity = _config_report_attribute_enabled("affinity");
			_report_activity = _config_report_attribute_enabled("activity");

			log("period_ms=",       _period_ms,       ", "
			    "report_activity=", _report_activity, ", "
			    "report_affinity=", _report_affinity);

			_timer.trigger_periodic(1000*_period_ms);
		}

		void _handle_period()
		{
			_update_subjects(_trace, _heap);
			_report_subjects(_report_affinity, _report_activity);
		}

		template <typename FUNC>
		void _for_each_subject(FUNC const &func)
		{
			for (Entry *entry = _entries.first(); entry; entry = entry->next()) {
				Trace::Subject_info info = _trace.subject_info(entry->id);
				func(entry->id, info);
			}
		}

		void _update_subjects(Trace::Connection &trace, Allocator &alloc)
		{
			unsigned const num_subjects = trace.subjects(_subjects, MAX_SUBJECTS);
			unsigned       num_traced   = num_subjects;

			/* add and update existing entries */
			for (unsigned i = 0; i < num_subjects; i++) {

				Trace::Subject_id const id = _subjects[i];

				Entry *e = _lookup_entry(id);
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
				if (e->info.state() == Trace::Subject_info::DEAD ||
				    !label_match)
				{
					trace.free(e->id);
					_entries.remove(e);
					destroy(alloc, e);
					log("do not trace \"", label, "\"");
					num_traced--;
				}
			}

			log("trace ", num_traced, " out of ", num_subjects, " subjects");
			_sort_by_recent_execution_time();


			/* enable tracing for test-thread */
			auto enable_tracing = [&] (Trace::Subject_id   id,
			                           Trace::Subject_info info) {

				while (Buffer_monitor *monitor = _monitors.first()) {
					_monitors.remove(monitor);
					destroy(alloc, monitor);
				}

				try {
					log("enable tracing for "
					    "thread:'", info.thread_name().string(), "' with "
					    "policy:", _policy_id.id
					);

					trace.trace(id.id, _policy_id, 16384U);

					Dataspace_capability ds_cap = trace.buffer(id.id);
					_monitors.insert(new (alloc) Buffer_monitor(_env.rm(), id.id, ds_cap));

				} catch (Trace::Source_is_dead) {
					error("source is dead");
					throw -2;
				}
			};

			_for_each_subject(enable_tracing);
		}

		void _report_subjects(bool report_affinity, bool report_activity)
		{
			log("");
			log("--- Report #", _report_cnt++, " ---");
			for (Entry const *e = _entries.first(); e; e = e->next()) {

				typedef Trace::Subject_info Subject_info;
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

	public:

		Main(Env &env) : _env(env)
		{
			_config_rom.sigh(_config_handler);
			_handle_config();
			_timer.sigh(_periodic_handler);

			try {
//				Rom_connection policy_rom(env, policy_module.string());
//				policy_module_rom_ds = policy_rom.dataspace();
//				size_t rom_size = Dataspace_client(policy_module_rom_ds).size();
//				policy_id = trace.alloc_policy(rom_size);
				Dataspace_capability policy_dst_ds = _trace.policy(_policy_id);

				if (policy_dst_ds.valid()) {
					void *dst = env.rm().attach(policy_dst_ds);
					void *src = env.rm().attach(_policy_ds);
					memcpy(dst, src, _policy_size);
					env.rm().detach(dst);
					env.rm().detach(src);
				}
				log("load module: '", _policy_name, "' for "
				    "label: '", _policy_label, "'");
			} catch (...) {
				error("could not load module '", _policy_name, "' for "
				      "label '", _policy_label, "'");
				throw;
			}
		}
};


void Component::construct(Env &env) { static Main main(env); }
