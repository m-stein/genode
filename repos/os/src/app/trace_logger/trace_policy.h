/*
 * \brief  Installs and maintains a tracing policy
 * \author Martin Stein
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_POLICY_H_
#define _TRACE_POLICY_H_

/* Genode includes */
#include <rom_session/connection.h>
#include <trace_session/connection.h>
#include <dataspace/client.h>


/**
 * Installs and maintains a tracing policy
 */
class Trace_policy
{
	public:

		using Name = Genode::String<32>;

	private:

		Genode::Env                            &_env;
		Genode::Trace::Connection              &_trace;
		Name                             const  _name;
		Genode::Session_label            const  _label { "init -> test-trace_logger" };
		Genode::Rom_connection                  _rom   { _env, _name.string() };
		Genode::Rom_dataspace_capability const  _ds    { _rom.dataspace() };
		Genode::size_t                   const  _size  { Genode::Dataspace_client(_ds).size() };
		Genode::Trace::Policy_id         const  _id    { _trace.alloc_policy(_size) };

	public:


		Trace_policy(Genode::Env               &env,
		             Genode::Trace::Connection &trace,
		             Name                       name);


		/***************
		 ** Accessors **
		 ***************/

		Genode::Trace::Policy_id id() const { return _id; }
};


#endif /* _TRACE_POLICY_H_ */
