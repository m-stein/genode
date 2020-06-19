/*
 * \brief  inotify handling for underlying file system.
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

/* local includes */
#include "notifier.h"


namespace Lx_fs
{
	enum {
		STACK_SIZE     = 8 * 1024,
		EVENT_SIZE     = (sizeof (struct inotify_event)),
		EVENT_BUF_LEN  = (1024 * (EVENT_SIZE + NAME_MAX + 1))
	};
}


bool Lx_fs::Notifier::_watched(char const *path)
{
	for (Entry *e=_watched_nodes.first(); e!=nullptr; e=e->next()) {
		if (e->path == path) return true;
	}

	return false;
}


void Lx_fs::Notifier::_add_to_watched(char const *path)
{
	auto watch_fd { inotify_add_watch(_fd, path,
	                                  IN_MODIFY | IN_CREATE | IN_DELETE) };

	if (watch_fd > 0) {
		Entry *elem { new (_heap) Entry { watch_fd, path } };

		_watched_nodes.insert(elem);
	}
}


int Lx_fs::Notifier::_add_cap(char const *path, Signal_context_capability cap)
{
	int wd { -1 };
	auto add_fn = [&] (Entry &elem) {
		if (elem.path == path) {
			Cap_entry *c { new (_heap) Cap_entry { cap } };
			elem.add_capabilty(c);
			wd = elem.watch_fd;
		}
	};

	for_each(add_fn);

	if(wd == -1) throw File_system::Lookup_failed { };

	return wd;
}


Lx_fs::Notifier::Notifier(Env &env)
:
	Thread { env, "inotify", STACK_SIZE },
	_heap { env.ram(), env.rm() }
{
	_fd = inotify_init();

	if (0 > _fd) {
		throw Init_notify_failed { };
	}

	start();
}


Lx_fs::Notifier::~Notifier()
{
	Entry *e { _watched_nodes.first() };
	while (e != nullptr) {
		Entry *r = e;
		e = e->next();

		auto rm_cap_fn = [&] (Cap_entry &c) {
			r->remove_capability(&c);
			destroy(_heap, &c);
		};

		_watched_nodes.remove(r);
		r->for_each_capability(rm_cap_fn);

		destroy(_heap, r);
	}

	close(_fd);
}


void Lx_fs::Notifier::entry()
{
	struct inotify_event* event { nullptr };

	auto watch_fn = [&] (Entry &elem) {

		if (elem.watch_fd == event->wd) {
			elem.for_each_capability([&] (Cap_entry cap) {
				Signal_transmitter(cap.cap).submit();
			});
		}
	};

	char buffer[EVENT_BUF_LEN] { 0 };
	while (true) {
		ssize_t const length { read(_fd, buffer, EVENT_BUF_LEN) };
		ssize_t       pos    { 0 };

		while (pos < length) {
			event = reinterpret_cast<struct inotify_event*>(&buffer[pos]);

			if ((event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)) {
				for_each(watch_fn);
			}
			pos += EVENT_SIZE + event->len;
		}
	}
}


int Lx_fs::Notifier::add_watch(const char* path, Signal_context_capability cap)
{
	if (!_watched(path)) {
		_add_to_watched(path);
	}

	return _add_cap(path, cap);
}


void Lx_fs::Notifier::remove_watch(char const *path, Signal_context_capability cap)
{
	auto remove_fn = [&] (Entry &elem) {
		if (elem.path == path) {

			elem.for_each_capability([&] (Cap_entry &c) {
				if (cap == c.cap) {
					elem.remove_capability(&c);
				}
			});
		}
	};

	for_each(remove_fn);
}
