/*
 * \brief  Log information about trace subjects
 * \author Martin Stein
 * \date   2018-01-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <trace_policy.h>
#include <monitor.h>

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>

using namespace Genode;
using Thread_name = String<32>;


class Main
{
	private:

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

		struct No_matching_monitor : Exception { };

		Env                           &_env;
		Timer::Connection              _timer            { _env };
		Trace::Connection              _trace            { _env, 1024*1024*10, 64*1024, 0 };
		Attached_rom_dataspace         _config_rom       { _env, "config" };
		Xml_node                       _config           { _config_rom.xml() };
		bool const                     _affinity         { _config.attribute_value("affinity", false) };
		bool const                     _activity         { _config.attribute_value("activity", false) };
		bool                           _verbose          { _config.attribute_value("verbose",  false) };
		Microseconds                   _period_us        { _config.attribute_value("period_ms", (unsigned long)DEFAULT_PERIOD_MS) * 1000UL };
		Timer::Periodic_timeout<Main>  _period           { _timer, *this, &Main::_handle_period, _period_us };
		Heap                           _heap             { _env.ram(), _env.rm() };
		Monitor_tree                   _monitors         { };
		Trace_policy                   _policy           { _env, _trace,
		                                                   _config.attribute_value("trace_policy", Trace_policy::Name("null")) };
		unsigned long                  _report_id        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];
		Trace::Subject_id              _traced_subjects[MAX_SUBJECTS];
		unsigned long                  _num_subjects     { 0 };
		unsigned long                  _num_monitors     { 0 };

		void _handle_period(Duration)
		{
			_num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS);
			for (unsigned i = 0; i < _num_subjects; i++) {
				Trace::Subject_id const id = _subjects[i];
				try { _monitors.find_by_subject_id(id); }
				catch (Monitor_tree::No_match) {
					try {
						if (!_subject_matches_policy(id))
							continue;

						_new_monitor(id);
					}
					catch (Trace::Source_is_dead) {
						warning("Failed to enable tracing");
					}
				}
			}
			log("");
			log("--- Report ", _report_id++, " (", _num_monitors, "/", _num_subjects, " subjects) ---");
			_monitors.for_each([&] (Monitor &monitor) {
				monitor.print(_activity, _affinity);
			});
		}

		void _destroy_monitor(Monitor &monitor)
		{
			if (_verbose)
				log("destroy monitor: subject ", monitor.subject_id().id);

			_trace.free(monitor.subject_id());
			_monitors.remove(&monitor);
			destroy(_heap, &monitor);
			_num_monitors--;
		}

		void _new_monitor(Trace::Subject_id id)
		{
			_trace.trace(id.id, _policy.id(), 16384U);
			Monitor &monitor =
				*new (_heap) Monitor(_trace, _env.rm(), id);

			_monitors.insert(&monitor);
			_num_monitors++;
			if (_verbose)
				log("new monitor: subject ", id.id);
		}

		bool _subject_matches_policy(Trace::Subject_id id)
		{
			Trace::Subject_info info = _trace.subject_info(id);
			Session_label const label(info.session_label());
			try {
				Session_policy policy(label, _config);
				if (!policy.has_attribute("thread"))
					return true;

				return policy.attribute_value("thread", Thread_name()) ==
				       info.thread_name();
			}
			catch (Session_policy::No_policy_defined) { }
			return false;
		}

	public:

		Main(Env &env) : _env(env) { }
};


void Component::construct(Env &env) { static Main main(env); }
