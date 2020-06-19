/*
 * \brief  inotify handling for underlying file system.
 * \author Pirmin Duss
 * \date   2020-06-17
 *
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOTIFIER_H_
#define _NOTIFIER_H_

/* Genode includes */
#include <base/exception.h>
#include <base/heap.h>
#include <base/signal.h>
#include <base/thread.h>
#include <file_system/listener.h>
#include <util/list.h>

/* local includes */
#include "lx_util.h"


namespace Lx_fs
{
	using namespace Genode;

	class Init_notify_failed : public Genode::Exception { };

	class Notifier;
}


class Lx_fs::Notifier final : public Thread
{
	private:

		struct Cap_entry : public Genode::List<Cap_entry>::Element
		{
			Signal_context_capability const cap;

			Cap_entry(Signal_context_capability const cap) : cap { cap } {}
		};

		struct Entry : public Genode::List<Entry>::Element
		{
			private:

				List<Cap_entry> _caps { };

			public:

				int         const  watch_fd;
				Path_string const  path;

				Entry(int const watch_fd, char const *path)
				:
					watch_fd { watch_fd }, path { path }
				{ }

				~Entry()
				{
					if (_caps.first() != nullptr)
						error("list of capabilities not empty at destruction time!");
				}

				template <typename FN>
				void for_each_capability(FN const &fn)
				{
					for (Cap_entry *e=_caps.first(); e!=nullptr; e=e->next()) {
						fn(*e);
					}
				}

				void add_capabilty(Cap_entry *cap_entry)
				{
					_caps.insert(cap_entry);
				}

				void remove_capability(Cap_entry *cap_entry)
				{
					_caps.remove(cap_entry);
				}
		};

		Heap         _heap;
		int          _fd { -1 };
		List<Entry>  _watched_nodes { };

		void entry() override;

		template <typename FN>
		void for_each(FN &fn)
		{
			for (Entry *e=_watched_nodes.first(); e!=nullptr; e=e->next()) {
				fn(*e);
			}
		}

		bool _watched(char const *path);
		void _add_to_watched(char const *path);
		int _add_cap(char const *path, Signal_context_capability cap);

	public:

		Notifier(Env &env);
		~Notifier();

		int add_watch(const char *path, Signal_context_capability cap);
		void remove_watch(char const *path, Signal_context_capability cap);
};

#endif  /* _NOTIFIER_H_ */
