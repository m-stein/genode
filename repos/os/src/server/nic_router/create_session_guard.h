/*
 * \brief  Guarding required objects during the process of creating a session
 * \author Martin Stein
 * \date   2023-07-18
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CREATE_SESSION_GUARD_H_
#define _CREATE_SESSION_GUARD_H_

/* nic_router includes */
#include <session_env.h>

namespace Net {

	using namespace Genode;

	class Create_session_guard;
}


class Net::Create_session_guard
{
	private:

		Constructible<Session_env> _tmp_session_env { };
		Constructible<Ram_dataspace_capability> _ram_ds { };
		void *_ram_ptr { nullptr };
		Session_env *_session_env_ptr { nullptr };

	public:

		template <typename SESSION_COMPONENT, typename CREATE_SESSION_FN>
		SESSION_COMPONENT *create_session(Env &env,
		                                  Quota &shared_quota,
		                                  char const *session_args,
		                                  CREATE_SESSION_FN && create_session_fn)
		{
			/*
			 * Note that this cannot be done in the constructor of this class
			 * because it is required that the destructor of this class is called
			 * on any exception that is thrown by the below code.
			 */

			/* create session env as temporary member of the guard object */
			_tmp_session_env.construct(env, shared_quota,
				Ram_quota { Arg_string::find_arg(session_args, "ram_quota").ulong_value(0) },
				Cap_quota { Arg_string::find_arg(session_args, "cap_quota").ulong_value(0) });

			/* alloc/attach ram ds and move session env to the base of it */
			_ram_ds.construct(_tmp_session_env->alloc(sizeof(Session_env) + sizeof(SESSION_COMPONENT), CACHED));
			_ram_ptr = _tmp_session_env->attach(*_ram_ds);
			_session_env_ptr = construct_at<Session_env>(_ram_ptr, *_tmp_session_env);
			SESSION_COMPONENT *session { create_session_fn(*_session_env_ptr, (void*)((addr_t)_ram_ptr + sizeof(Session_env)), *_ram_ds) };

			/* ensure that the ram ds is not dissolved on guard destruction */
			_tmp_session_env.destruct();
			_session_env_ptr = nullptr;
			return session;
		}

		~Create_session_guard()
		{
			if (_session_env_ptr) {

				Session_env tmp_session_env { *_session_env_ptr };
				if (_ram_ds.constructed())
					tmp_session_env.free(*_ram_ds);
				if (_ram_ptr)
					tmp_session_env.detach(_ram_ptr);

			} else if (_tmp_session_env.constructed()) {

				if (_ram_ds.constructed())
					_tmp_session_env->free(*_ram_ds);
				if (_ram_ptr)
					_tmp_session_env->detach(_ram_ptr);
			}
		}
};


#endif /* _CREATE_SESSION_GUARD_H_ */
