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

/* local includes */
#include <avl_tree.h>

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
class Monitor : public Local::Avl_node<Monitor>
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


		/**************
		 ** Avl_node **
		 **************/

		Monitor &find_by_subject_id(Trace::Subject_id const subject_id);

		bool higher(Monitor *monitor) { return monitor->_subject_id.id > _subject_id.id; }


		/***************
		 ** Accessors **
		 ***************/

		Trace::Subject_id          subject_id() const { return _subject_id; }
		Trace::Subject_info const &info()       const { return _info; }
};


/**
 * AVL tree of monitors with their subject ID as index
 */
struct Monitor_tree : Local::Avl_tree<Monitor>
{
	struct No_match : Exception { };

	Monitor &find_by_subject_id(Trace::Subject_id const subject_id);
};


/*************
 ** Monitor **
 *************/

Monitor &Monitor::find_by_subject_id(Trace::Subject_id const subject_id)
{
	if (subject_id.id == _subject_id.id) {
		return *this; }

	bool const side = subject_id.id > _subject_id.id;
	Monitor *const monitor = Avl_node<Monitor>::child(side);
	if (!monitor) {
		throw Monitor_tree::No_match(); }

	return monitor->find_by_subject_id(subject_id);
}


/******************
 ** Monitor_tree **
 ******************/

Monitor &Monitor_tree::find_by_subject_id(Trace::Subject_id const subject_id)
{
	Monitor *const monitor = first();
	if (!monitor)
		throw No_match();

	return monitor->find_by_subject_id(subject_id);
}


class Trace_policy
{
	private:

		Env                            &_env;
		Trace::Connection              &_trace;
		String<32>               const  _name  { "null" };
		Session_label            const  _label { "init -> test-trace_logger" };
		Rom_connection                  _rom   { _env, _name.string() };
		Rom_dataspace_capability const  _ds    { _rom.dataspace() };
		size_t                   const  _size  { Dataspace_client(_ds).size() };
		Trace::Policy_id         const  _id    { _trace.alloc_policy(_size) };

	public:

		Trace_policy(Env &env, Trace::Connection &trace)
		:
			_env(env), _trace(trace)
		{
			try {
				Dataspace_capability dst_ds = _trace.policy(_id);
				void *dst = _env.rm().attach(dst_ds);
				void *src = _env.rm().attach(_ds);
				memcpy(dst, src, _size);
				_env.rm().detach(dst);
				_env.rm().detach(src);

				log("load module: '", _name,  "'"
				     " for label: '", _label, "'");
			} catch (Region_map::Invalid_dataspace) {

				error("Failed to load policy '", _name,  "'"
				      "            for label '", _label, "'");
				throw;
			}
		}


		/***************
		 ** Accessors **
		 ***************/

		Trace::Policy_id id() const { return _id; }
};


class Main
{
	private:

		using Thread_name = String<32>;

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

		struct No_matching_monitor : Exception { };

		Env                           &_env;
		Trace::Connection              _trace            { _env, 1024*1024*10, 64*1024, 0 };
		Attached_rom_dataspace         _config_rom       { _env, "config" };
		Xml_node                       _config           { _config_rom.xml() };
		bool const                     _affinity         { _config.attribute_value("affinity", false) };
		bool const                     _activity         { _config.attribute_value("activity", false) };
		Timer::Connection              _timer            { _env };
		Heap                           _heap             { _env.ram(), _env.rm() };
		Monitor_tree                   _monitors         { };
		Trace_policy                   _policy           { _env, _trace };
		unsigned long                  _report_id        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];
		Trace::Subject_id              _traced_subjects[MAX_SUBJECTS];
		Microseconds                   _period_us        { _config.attribute_value("period_ms", (unsigned long)DEFAULT_PERIOD_MS) * 1000UL };
		Timer::Periodic_timeout<Main>  _period           { _timer, *this, &Main::_handle_period, _period_us };
		unsigned long                  _num_subjects     { 0 };
		unsigned long                  _num_monitors     { 0 };

		void _handle_period(Duration)
		{
			_num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS);
			for (unsigned i = 0; i < _num_subjects; i++) {
				Trace::Subject_id const id = _subjects[i];
				try {
					_update_monitor(_monitors.find_by_subject_id(id), id);
				}
				catch (Monitor_tree::No_match) {
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
			log("");
			log("--- Report #", _report_id++, " (", _num_monitors, "/", _num_subjects, " subjects) ---");
			_monitors.for_each([&] (Monitor &monitor) {
				monitor.print(_activity, _affinity);
			});
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
			_trace.trace(id.id, _policy.id(), 16384U);
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
			if (monitor.info().state() == Trace::Subject_info::DEAD)
				_destroy_monitor(monitor);
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
