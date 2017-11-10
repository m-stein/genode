/*
 * \brief  Diversified test of the Register and MMIO framework
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <trace_session/connection.h>
#include <timer_session/connection.h>
#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/sleep.h>

using namespace Genode;


class Trace_buffer_monitor
{
	private:

		enum { MAX_ENTRY_BUF = 256 };

		char                  _buf[MAX_ENTRY_BUF];
		Region_map           &_rm;
		Trace::Subject_id     _id;
		Trace::Buffer        *_buffer;
		Trace::Buffer::Entry  _curr_entry;

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
			_rm(rm), _id(id), _buffer(rm.attach(ds_cap)),
			_curr_entry(_buffer->first())
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
			log("overflows: ", _buffer->wrapped());
			log("read all remaining events");

unsigned xxx = 0;
			for (; !_curr_entry.last(); _curr_entry = _buffer->next(_curr_entry)) {
log("entry ", xxx++);
				/* omit empty entries */
				if (_curr_entry.length() == 0)
					continue;

				char const * const data = _terminate_entry(_curr_entry);
				if (data) { log(data); }
			}

			/* reset after we read all available entries */
			_curr_entry = _buffer->first();
		}
};


struct Main
{
	Env                     &env;
	Attached_rom_dataspace   config       { env, "config" };
	Trace::Connection        trace        { env, 1024*1024*10, 64*1024, 0 };
	Timer::Connection        timer        { env };
	Trace::Policy_id         policy_id;

	Constructible<Trace_buffer_monitor> test_monitor;

	typedef Genode::String<64> String;
	String                    policy_label  { "init -> client" };
	String                    policy_thread { "ep" };
	String                    policy_module { "rpc_name" };
	Rom_dataspace_capability  policy_module_rom_ds;

	template <typename FUNC>
	void for_each_subject(Trace::Subject_id subjects[],
	                      size_t max_subjects, FUNC const &func)
	{
		for (size_t i = 0; i < max_subjects; i++) {
			Trace::Subject_info info = trace.subject_info(subjects[i]);
			func(subjects[i].id, info);
		}
	}

	char const *state_name(Trace::Subject_info::State state)
	{
		switch (state) {
		case Trace::Subject_info::INVALID:  return "INVALID";
		case Trace::Subject_info::UNTRACED: return "UNTRACED";
		case Trace::Subject_info::TRACED:   return "TRACED";
		case Trace::Subject_info::FOREIGN:  return "FOREIGN";
		case Trace::Subject_info::ERROR:    return "ERROR";
		case Trace::Subject_info::DEAD:     return "DEAD";
		}
		return "undefined";
	}

	Main(Env &env) : env(env)
	{
		log("Start Trace Monitor");

		try {
			Rom_connection policy_rom(env, policy_module.string());
			policy_module_rom_ds = policy_rom.dataspace();

			size_t rom_size = Dataspace_client(policy_module_rom_ds).size();

			policy_id = trace.alloc_policy(rom_size);
			Dataspace_capability ds_cap = trace.policy(policy_id);

			if (ds_cap.valid()) {
				void *ram = env.rm().attach(ds_cap);
				void *rom = env.rm().attach(policy_module_rom_ds);
				memcpy(ram, rom, rom_size);

				env.rm().detach(ram);
				env.rm().detach(rom);
			}

			log("load module: '", policy_module, "' for "
			    "label: '", policy_label, "'");
		} catch (...) {
			error("could not load module '", policy_module, "' for "
			      "label '", policy_label, "'");
			throw;
		}

		/* wait some time before querying the subjects */
		timer.msleep(3000);

		Trace::Subject_id subjects[32];
		size_t num_subjects = trace.subjects(subjects, 32);

		log(num_subjects, " tracing subjects present");

		auto print_info = [this] (Trace::Subject_id id, Trace::Subject_info info) {

			log("ID:",      id.id,                    " "
			    "label:\"", info.session_label(),   "\" "
			    "name:\"",  info.thread_name(),     "\" "
			    "state:",   state_name(info.state()), " "
			    "policy:",  info.policy_id().id,      " "
			    "time:",    info.execution_time().value);
		};

		for_each_subject(subjects, num_subjects, print_info);

		/* enable tracing for test-thread */
		auto enable_tracing = [this, &env] (Trace::Subject_id id,
		                                    Trace::Subject_info info) {

			if (   info.session_label() != policy_label
			    || info.thread_name()   != policy_thread) {
				return;
			}

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

		/* give the test thread some time to run */
		timer.msleep(5000);

		for_each_subject(subjects, num_subjects, print_info);

		/* read events from trace buffer */
		if (test_monitor.constructed()) {
			test_monitor->dump();
			test_monitor.destruct();
		}

		log("passed Tracing test");
	}
};


void Component::construct(Genode::Env &env)
{
	static Main main(env);
}

