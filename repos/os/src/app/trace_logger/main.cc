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


/**
 * Wrapper for Trace::Buffer that adds some convenient functionality
 */
class Buffer
{
	private:

		Trace::Buffer        &_buffer;
		Trace::Buffer::Entry  _curr { 0 };

	public:

		Buffer(Trace::Buffer &buffer) : _buffer(buffer) { }

		/**
		 * Call functor for each entry that wasn't yet processed
		 */
		template <typename FUNC>
		void for_each_new_entry(FUNC && functor)
		{
			/* initialize _curr if _buffer was empty until now */
			if (_curr.last())
				_curr = _buffer.first();

			/* iterate over all entries that were not processed yet */
			     Trace::Buffer::Entry e1 = _curr;
			for (Trace::Buffer::Entry e2 = _curr; !e2.last(); e2 = _buffer.next(e2))
			{
				e1 = e2;
				functor(e1);
			}
			/* remember the last processed entry in _curr */
			_curr = e1;
		}
};


/**
 * Monitors tracing information of one tracing subject
 */
class Monitor : public List<Monitor>::Element
{
	private:

		enum { MAX_ENTRY_LENGTH = 256 };

		Trace::Subject_id const  _subject_id;
		Region_map              &_rm;
		Buffer                   _buffer;
		unsigned long            _report_id        { 0 };
		Trace::Subject_info      _info             { };
		unsigned long long       _recent_exec_time { 0 };
		char                     _curr_entry_data[MAX_ENTRY_LENGTH];

	public:

		Monitor(Trace::Subject_id     subject_id,
		        Region_map           &rm,
		        Dataspace_capability  buffer_ds)
		:
			_subject_id(subject_id), _rm(rm),
			_buffer(*(Trace::Buffer *)rm.attach(buffer_ds))
		{
			log("new monitor: subject ", _subject_id.id, " buffer ", &_buffer);
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

		void print(bool report_activity, bool report_affinity)
		{
			/* print general subject information */
			typedef Trace::Subject_info Subject_info;
			Subject_info::State const state = _info.state();
			log("<subject label=\"",  _info.session_label().string(),
			          "\" thread=\"", _info.thread_name().string(),
			          "\" id=\"",     _subject_id.id,
			          "\" state=\"",  Subject_info::state_name(state),
			          "\">");

			/* print subjects activity if desired */
			if (report_activity)
				log("   <activity total=\"",  _info.execution_time().value,
				              "\" recent=\"", _recent_exec_time,
				              "\">");

			/* print subjects affinity if desired */
			if (report_affinity)
				log("   <affinity xpos=\"", _info.affinity().xpos(),
				              "\" ypos=\"", _info.affinity().ypos(),
				              "\">");

			/* print all buffer entries that we haven't yet printed */
			bool printed_buf_entries = false;
			_buffer.for_each_new_entry([&] (Trace::Buffer::Entry entry) {

				/* get readable data length and skip empty entries */
				size_t length = min(entry.length(), (unsigned)MAX_ENTRY_LENGTH - 1);
				if (!length)
					return;

				/* copy entry data from buffer and add terminating '0' */
				memcpy(_curr_entry_data, entry.data(), length);
				_curr_entry_data[length] = '\0';

				/* print copied entry data out to log */
				if (!printed_buf_entries) {
					log("   <buffer>");
					printed_buf_entries = true;
				}
				log(Cstring(_curr_entry_data));
			});
			/* print end tags */
			if (printed_buf_entries)
				log("   </buffer>");
			else
				log("   <buffer />");
			log("</subject>");
		}


		/***************
		 ** Accessors **
		 ***************/

		Trace::Subject_id          subject_id() const { return _subject_id; }
		Trace::Subject_info const &info()       const { return _info; }
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
		String<32>                     _policy_name      { "null" };
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
		unsigned long                  _num_subjects     { 0 };
		unsigned long                  _num_monitors     { 0 };

		Monitor &_lookup_monitor(Trace::Subject_id const id)
		{
			for (Monitor *monitor = _monitors.first(); monitor;
			     monitor = monitor->List<Monitor>::Element::next())
			{
				if (monitor->subject_id() == id)
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
			_update_monitors();
			_report_subjects();
		}

		void _destroy_monitor(Monitor &monitor)
		{
			_trace.free(monitor.subject_id());
			_monitors.remove(&monitor);
			destroy(_heap, &monitor);
			_num_monitors--;
		}

		void _new_monitor(Trace::Subject_id id)
		{
			_trace.trace(id.id, _policy_id, 16384U);
			Monitor &monitor =
				*new (_heap) Monitor(id, _env.rm(),
				                     _trace.buffer(id));

			_monitors.insert(&monitor);
			_update_monitor(monitor, id);
			_num_monitors++;
		}

		void _update_monitor(Monitor &monitor, Trace::Subject_id id)
		{
			monitor.update(_trace.subject_info(id));

			/* purge dead threads */
			if (monitor.info().state() == Trace::Subject_info::DEAD)
			{
				_destroy_monitor(monitor);
			}
		}

		bool _subject_matches_policy(Trace::Subject_id id)
		{
			Trace::Subject_info info = _trace.subject_info(id);
			Session_label const label(info.session_label(), " -> ",
			                          info.thread_name().string());
			try {
				Session_policy policy(label, _config);
				return true;
			}
			catch (Session_policy::No_policy_defined) {
				return false;
			}
		}

		void _update_monitors()
		{
			_num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS);
			for (unsigned i = 0; i < _num_subjects; i++) {
				Trace::Subject_id const id = _subjects[i];
				try { _update_monitor(_lookup_monitor(id), id); }
				catch (No_matching_monitor) {
					try {
						if (!_subject_matches_policy(id))
							continue;

						_new_monitor(id);
					}
					catch (Trace::Source_is_dead) {
						error("Failed to enable tracing");
					}
				}
			}
			log("trace ", _num_monitors, " out of ", _num_subjects, " subjects");
		}

		void _report_subjects()
		{
			log("");
			log("--- Report #", _report_id++, " ---");

			for (Monitor *monitor = _monitors.first(); monitor;
			     monitor = monitor->next())
			{
				monitor->print(_report_activity, _report_affinity);
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
