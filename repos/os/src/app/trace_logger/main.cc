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
#include <util/construct_at.h>

using namespace Genode;
using Thread_name = String<32>;


class Main
{
	private:

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

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
		Monitor_tree                   _monitors_0       { };
		Monitor_tree                   _monitors_1       { };
		unsigned                       _monitors_id      { 0 };
		Trace_policy                   _policy           { _env, _trace,
		                                                   _config.attribute_value("trace_policy", Trace_policy::Name("null")) };
		unsigned long                  _report_id        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];
		Trace::Subject_id              _traced_subjects[MAX_SUBJECTS];
		unsigned long                  _num_subjects     { 0 };
		unsigned long                  _num_monitors     { 0 };

		void _handle_period(Duration)
		{
			/*
			 * Update monitors
			 *
			 * Which monitors are held and how they are configured depends on:
			 *
			 *   1) Which subjects are available at the Trace session,
			 *   2) which tracing state the subjects are currently in,
			 *   3) the configuration of this component about which subjects
			 *      to monitor and how
			 *
			 * All this might have changed since the last call of this method.
			 * So, adapt the monitors and the monitor tree accordingly.
			 */

			/*
			 * Switch monitor trees so that the new tree is empty and the old
			 * tree contains all monitors.
			 */
			Monitor_tree &old_monitors = _monitors_id ? _monitors_1 : _monitors_0;
			Monitor_tree &new_monitors = _monitors_id ? _monitors_0 : _monitors_1;
			_monitors_id = !_monitors_id;

			/* update array of available subject IDs and iterate over them */
			try { _num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS); }
			catch (Out_of_ram ) { warning("Cannot list subjects: Out_of_ram" ); return; }
			catch (Out_of_caps) { warning("Cannot list subjects: Out_of_caps"); return; }
			for (unsigned i = 0; i < _num_subjects; i++) {

				/* skip dead subjects */
				Trace::Subject_id const id = _subjects[i];
				try {
					if (_trace.subject_info(id).state() == Trace::Subject_info::DEAD)
						continue;
				}
				catch (Trace::Nonexistent_subject) { continue; }
				try {
					/* lookup monitor for subject ID */
					Monitor &monitor = old_monitors.find_by_subject_id(id);

					/* if monitor is deprecated, leave it in the old tree */
					if (!_subject_matches_policy(id))
						continue;

					/* move monitor from old to new tree */
					old_monitors.remove(&monitor);
					new_monitors.insert(&monitor);

				} catch (Monitor_tree::No_match) {

					/* monitor only subjects the user is interested in */
					if (!_subject_matches_policy(id))
						continue;

					/* create a monitor in the new tree for the subject */
					_new_monitor(new_monitors, id);
				}
			}
			/* all monitors in the old tree are deprecated, destroy them */
			while (Monitor *monitor = old_monitors.first())
				_destroy_monitor(old_monitors, *monitor);

			/* dump information of each monitor left */
			log("");
			log("--- Report ", _report_id++, " (", _num_monitors, "/", _num_subjects, " subjects) ---");
			new_monitors.for_each([&] (Monitor &monitor) {
				monitor.print(_activity, _affinity);
			});
		}

		void _destroy_monitor(Monitor_tree &monitors, Monitor &monitor)
		{
			if (_verbose)
				log("destroy monitor: subject ", monitor.subject_id().id);

			try { _trace.free(monitor.subject_id()); }
			catch (Trace::Nonexistent_subject) { }
			monitors.remove(&monitor);
			destroy(_heap, &monitor);
			_num_monitors--;
		}

		void _new_monitor(Monitor_tree &monitors, Trace::Subject_id id)
		{
			try {
				_trace.trace(id.id, _policy.id(), 16384U);
				monitors.insert(new (_heap) Monitor(_trace, _env.rm(), id));
			}
			catch (Out_of_ram                    ) { warning("Cannot activate tracing: Out_of_ram"             ); return; }
			catch (Out_of_caps                   ) { warning("Cannot activate tracing: Out_of_caps"            ); return; }
			catch (Trace::Already_traced         ) { warning("Cannot activate tracing: Already_traced"         ); return; }
			catch (Trace::Source_is_dead         ) { warning("Cannot activate tracing: Source_is_dead"         ); return; }
			catch (Trace::Nonexistent_policy     ) { warning("Cannot activate tracing: Nonexistent_policy"     ); return; }
			catch (Trace::Traced_by_other_session) { warning("Cannot activate tracing: Traced_by_other_session"); return; }
			catch (Trace::Nonexistent_subject    ) { warning("Cannot activate tracing: Nonexistent_subject"    ); return; }
			_num_monitors++;
			if (_verbose)
				log("new monitor: subject ", id.id);
		}

		bool _subject_matches_policy(Trace::Subject_id id)
		{
			try {
				Trace::Subject_info info = _trace.subject_info(id);
				Session_label const label(info.session_label());
				Session_policy policy(label, _config);
				if (!policy.has_attribute("thread"))
					return true;

				return policy.attribute_value("thread", Thread_name()) ==
				       info.thread_name();
			}
			catch (Session_policy::No_policy_defined) { }
			catch (Trace::Nonexistent_subject       ) { }
			return false;
		}

	public:

		Main(Env &env) : _env(env) { }
};


void Component::construct(Env &env) { static Main main(env); }
