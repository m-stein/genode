/*
 * \brief  Frontend for controlling the TRACE session
 * \author Johannes Schlatow
 * \date   2021-08-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_CONTROL_H_
#define _TRACE_CONTROL_H_

#include <base/registry.h>
#include <os/session_policy.h>

#include "trace_buffer.h"
#include "subject_info.h"
#include "policy.h"

namespace Ctf {
	using namespace Genode;

	class  Trace_control;
}

class Ctf::Trace_control
{
	private:
		enum { MAX_SUBJECTS             = 512 };
		enum { DEFAULT_BUFFER_SIZE      = 64 * 1024 };
		enum { TRACE_SESSION_RAM        = 1024 * 1024 };
		enum { TRACE_SESSION_ARG_BUFFER = 4 * 1024 };

		struct Attached_buffer
		{
			Env                               &_env;
			Trace_buffer                       _buffer;
			Registry<Attached_buffer>::Element _element;
			Subject_info                       _info;

			Attached_buffer(Registry<Attached_buffer>    &registry,
			                Genode::Env                  &env,
			                Genode::Dataspace_capability ds,
			                Trace::Subject_info          &info)
			: _env(env),
			  _buffer(*((Trace::Buffer*)_env.rm().attach(ds))),
			  _element(registry, *this),
			  _info(info)
			{ }

			~Attached_buffer()
			{
				_env.rm().detach(_buffer.address());
			}

			Trace_buffer            &trace_buffer()       { return _buffer; }

			Subject_info      const &info()         const { return _info;   }
		};

		Env                       &_env;
		Allocator                 &_alloc;
		Registry<Attached_buffer>  _trace_buffers { };
		Policy_tree                _policies      { };

		Trace::Connection          _trace         { _env,
		                                            TRACE_SESSION_RAM,
		                                            TRACE_SESSION_ARG_BUFFER,
		                                            0 };

		unsigned long              _num_subjects  { 0 };
		Trace::Subject_id          _subjects[MAX_SUBJECTS];

		/* get matching session policy for given subject */
		Session_policy _session_policy(Trace::Subject_info const &info, Xml_node config)
		{
			Session_label const label(info.session_label());
			Session_policy policy(label, config);
			if (policy.has_attribute("thread"))
				if (policy.attribute_value("thread", Trace::Thread_name()) != info.thread_name())
					throw Session_policy::No_policy_defined();

			return policy;
		}

		/**
 		 * Look for matching subjects
 		 */
		void _find_and_start_subjects(Xml_node config)
		{
			/* update available subject IDs and iterate over them */
			try { _num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS); }
			catch (Out_of_ram ) { warning("Cannot list subjects: Out_of_ram" ); return; }
			catch (Out_of_caps) { warning("Cannot list subjects: Out_of_caps"); return; }
			for (unsigned i = 0; i < _num_subjects; i++) {

				Trace::Subject_id const id = _subjects[i];

				try {
					/* skip dead subjects */
					if (_trace.subject_info(id).state() == Trace::Subject_info::DEAD)
						continue;

					/* check if there is a matching policy in the XML config */
					Trace::Subject_info info      = _trace.subject_info(id);
					Session_policy session_policy = _session_policy(info, config);

					if (!session_policy.has_attribute("policy"))
						continue;

					Number_of_bytes buffer_sz =
						session_policy.attribute_value("buffer", Number_of_bytes(DEFAULT_BUFFER_SIZE));

					/* find and assign policy; create/insert if not present */
					Policy_name  const policy_name = session_policy.attribute_value("policy", Policy_name());
					try {
						_trace.trace(id, _policies.find_by_name(policy_name).id(), buffer_sz);
					} catch (Policy_tree::No_match) {
						Policy &policy = *new (_alloc) Policy(_env, _trace, policy_name);
						_policies.insert(policy);
						_trace.trace(id, policy.id(), buffer_sz);
					}

					/* attach and remember trace buffer */
					new (_alloc) Attached_buffer(_trace_buffers, _env, _trace.buffer(id), info);
				}
				catch (Trace::Nonexistent_subject       ) { continue; }
				catch (Session_policy::No_policy_defined) { continue; }
			}
		}

	public:

		Trace_control(Env &env, Allocator &alloc)
		: _env(env),
		  _alloc(alloc)
		{ }

		void start(Xml_node config)
		{
			stop();

			/* find matching subjects according to config and start tracing */
			_find_and_start_subjects(config);
		}

		void stop()
		{
			/* clear registry (detaching all buffers) */
			_trace_buffers.for_each([&] (Attached_buffer &buf) {
				destroy(_alloc, &buf);
			});

			/* pause all subjects */
			try { _num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS); }
			catch (Out_of_ram ) { warning("Cannot list subjects: Out_of_ram" ); return; }
			catch (Out_of_caps) { warning("Cannot list subjects: Out_of_caps"); return; }
			for (unsigned i = 0; i < _num_subjects; i++) {
				_trace.pause(_subjects[i]);
			}
		}

		template <typename FN>
		void for_each_buffer(FN const &fn)
		{
			_trace_buffers.for_each([&] (Attached_buffer &buf) {
				fn(buf.trace_buffer(), buf.info());
			});
		}
};


#endif /* _TRACE_CONTROL_H_ */
