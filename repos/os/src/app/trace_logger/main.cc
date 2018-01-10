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


class Monitor : public List<Monitor>::Element
{
	private:

		enum { MAX_ENTRY_BUF = 256 };

		Trace::Subject_id const  _id;
		char                     _buf[MAX_ENTRY_BUF];
		Region_map              &_rm;
		Trace::Buffer           &_buffer;
		unsigned long            _report_id { 0 };
		Trace::Subject_info      _info                  { };
		unsigned long long       _recent_exec_time { 0 };

		const char *_terminate_entry(Trace::Buffer::Entry const &entry)
		{
			size_t len = min(entry.length() + 1, (unsigned)MAX_ENTRY_BUF);
			memcpy(_buf, entry.data(), len);
			_buf[len-1] = '\0';
			return _buf;
		}

	public:

		Monitor(Trace::Subject_id     id,
		        Region_map           &rm,
		        Dataspace_capability  buffer_ds)
		:
			_id(id), _rm(rm),
			_buffer(*(Trace::Buffer *)rm.attach(buffer_ds))
		{
			log("new monitor: subject ", _id.id, " buffer ", &_buffer);
		}

		~Monitor()
		{
			_buffer.~Buffer();
			_rm.detach(&_buffer);
		}

		void update(Trace::Subject_info const &new_info)
		{
			unsigned long long const last_execution_time =
				_info.execution_time().value;

			_info = new_info;
			_recent_exec_time =
				_info.execution_time().value - last_execution_time;
		}

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


		/***************
		 ** Accessors **
		 ***************/

		Trace::Subject_id          id()               const { return _id; }
		unsigned long long         recent_exec_time() const { return _recent_exec_time; }
		unsigned long              report_id()        const { return _report_id; }
		Trace::Subject_info const &info()             const { return _info; }

		void incr_report_id() { _report_id++; }
};


class Main
{
	private:

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

		struct No_matching_monitor : Exception { };

		Env                           &_env;
		Trace::Connection              _trace            { _env, 1024*1024*10, 64*1024, 0 };
		bool                           _report_affinity  { false };
		bool                           _report_activity  { false };
		Attached_rom_dataspace         _config_rom       { _env, "config" };
		Xml_node                       _config           { _config_rom.xml() };
		Timer::Connection              _timer            { _env };
		Heap                           _heap             { _env.ram(), _env.rm() };
		List<Monitor>                  _monitors         { };
		String<32>                     _policy_name      { "rpc_name" };
		Rom_connection                 _policy_rom       { _env, _policy_name.string() };
		Rom_dataspace_capability       _policy_ds        { _policy_rom.dataspace() };
		Session_label                  _policy_label     { "init -> test-trace_logger" };
		size_t                         _policy_size      { Dataspace_client(_policy_ds).size() };
		Trace::Policy_id               _policy_id        { _trace.alloc_policy(_policy_size) };
		unsigned long                  _report_id        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];
		Trace::Subject_id              _traced_subjects[MAX_SUBJECTS];
		Signal_handler<Main>           _config_handler   { _env.ep(), *this, &Main::_handle_config};
		Microseconds                   _period_us        { _config.attribute_value("period_ms", (unsigned long)DEFAULT_PERIOD_MS) * 1000UL };
		Timer::Periodic_timeout<Main>  _period           { _timer, *this, &Main::_handle_period, _period_us };
		unsigned                       _num_traced       { 0 };

		Monitor &_lookup_monitor(Trace::Subject_id const id)
		{
			for (Monitor *monitor = _monitors.first(); monitor;
			     monitor = monitor->List<Monitor>::Element::next())
			{
				if (monitor->id() == id)
					return *monitor;
			}

			throw No_matching_monitor();
		}

		bool _config_report_attribute_enabled(char const *attr) const
		{
			try {
				return _config.sub_node("report").attribute_value(attr, false);
			} catch (...) { return false; }
		}

		void _handle_config()
		{
			error(__func__, " not implemented");
			_config_rom.update();
		}

		void _handle_period(Duration)
		{
			_update_subjects();
			_report_subjects();
		}

		void _update_monitor(Monitor &monitor, Trace::Subject_id id)
		{
			monitor.update(_trace.subject_info(id));

			bool label_match = false;
			Session_label const label(monitor.info().session_label(),
			                          " -> ",
			                          monitor.info().thread_name().string());
			try {
				Session_policy policy(label, _config);
				label_match = true;
			}
			catch (Session_policy::No_policy_defined) { }

			/* purge dead threads */
			if (monitor.info().state() == Trace::Subject_info::DEAD ||
			    !label_match)
			{
				_trace.free(monitor.id());
				_monitors.remove(&monitor);
				destroy(_heap, &monitor);
				_num_traced--;
			}
		}

		void _update_subjects()
		{
			unsigned const num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS);
			_num_traced = num_subjects;

			for (unsigned i = 0; i < num_subjects; i++) {

				Trace::Subject_id const id = _subjects[i];

				try { _update_monitor(_lookup_monitor(id), id); }
				catch (No_matching_monitor) {

					try {
						_trace.trace(id.id, _policy_id, 16384U);
						Dataspace_capability buffer_ds = _trace.buffer(id);
						Monitor &monitor =
							*new (_heap) Monitor(id, _env.rm(), buffer_ds);

						_monitors.insert(&monitor);
						_update_monitor(monitor, id);

					} catch (Trace::Source_is_dead) {

						error("Failed to enable tracing");
					}
				}
			}
			log("trace ", _num_traced, " out of ", num_subjects, " subjects");
		}

		void _report_subjects()
		{
			log("");
			log("--- Report #", _report_id++, " ---");
			for (Monitor const *monitor = _monitors.first(); monitor;
			     monitor = monitor->next())
			{
				typedef Trace::Subject_info Subject_info;
				Subject_info::State const state = monitor->info().state();
				log("<subject label=\"",  monitor->info().session_label().string(),
				          "\" thread=\"", monitor->info().thread_name().string(),
				          "\" id=\"",     monitor->id().id,
				          "\" state=\"",  Subject_info::state_name(state),
				          "\">");

				if (_report_activity)
					log("  <activity total=\"",  monitor->info().execution_time().value,
					             "\" recent=\"", monitor->recent_exec_time(),
					             "\">");

				if (_report_affinity)
					log("  <affinity xpos=\"", monitor->info().affinity().xpos(),
					             "\" ypos=\"", monitor->info().affinity().ypos(),
					             "\">");

				log("</subject>");
			}
		}

		void _install_policy()
		{
			try {
				Dataspace_capability policy_dst_ds = _trace.policy(_policy_id);

				if (policy_dst_ds.valid()) {
					void *dst = _env.rm().attach(policy_dst_ds);
					void *src = _env.rm().attach(_policy_ds);
					memcpy(dst, src, _policy_size);
					_env.rm().detach(dst);
					_env.rm().detach(src);
				}
				log("load module: '", _policy_name, "' for "
				    "label: '", _policy_label, "'");
			} catch (...) {
				error("could not load module '", _policy_name, "' for "
				      "label '", _policy_label, "'");
				throw;
			}
		}

	public:

		Main(Env &env) : _env(env)
		{
			_config_rom.sigh(_config_handler);
			_handle_config();
			_install_policy();
		}
};


void Component::construct(Env &env) { static Main main(env); }
