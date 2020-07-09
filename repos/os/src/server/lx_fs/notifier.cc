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
		EVENT_BUF_LEN  = (1024 * (EVENT_SIZE + NAME_MAX + 1)),
		PARALELL_NOTIFICATIONS = 4,
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
	auto add_fn = [&cap, &path, &wd, this] (Entry &elem) {
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


void Lx_fs::Notifier::_add_notify(Signal_context_capability cap)
{
	for (auto const *e = _notify_queue.first(); e != nullptr; e = e->next()) {
		if (e->cap == cap) {
			return;
		}
	}

	auto *entry { new (_heap) Cap_entry { cap } };
	_notify_queue.insert(entry);

	if (!_notify_timer_running) {
		_notify_timer_running = true;
		_notify_timer.trigger_once(5000);
	}
}


void Lx_fs::Notifier::_process_notify()
{
	_notify_timer_running = false;

	/**
	 * limit amount of watch events sent at the same time,
	 * to prevent an overflow of the packet ack queue of the
	 * File_system session.
	 */
	int cnt { 0 };
	auto *e = _notify_queue.first();
	while ((e != nullptr) && (cnt < PARALELL_NOTIFICATIONS)) {
		Signal_transmitter {e->cap}.submit();
		++cnt;

		auto *tmp = e;
		e = e->next();
		_notify_queue.remove(tmp);
	}

	if (_notify_queue.first() != nullptr) {
		_notify_timer_running = true;
		_notify_timer.trigger_once(5000);
	}
}


Lx_fs::Notifier::Notifier(Env &env)
:
	Thread { env, "inotify", STACK_SIZE },
	_env   { env }
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


		r->remove_all(_heap, [ ] (Signal_context_capability cap) {
			Signal_transmitter {cap}.submit();
		});

		_watched_nodes.remove(r);

		destroy(_heap, r);
	}

	close(_fd);
}


void Lx_fs::Notifier::entry()
{
	struct inotify_event *event { nullptr };

	auto modify_fn = [&event] (Entry &elem) {
		if (elem.watch_fd == event->wd) {
			elem.notify_all([ ] (Signal_context_capability cap) {
				Signal_transmitter {cap}.submit();
			});
		}
	};

	auto ignore_fn = [&event, this] (Entry &elem) {

		if (elem.watch_fd == event->wd) {

			elem.remove_all(_heap, [ ] (Signal_context_capability cap) {

				/* notify clients something has changed */
				Signal_transmitter {cap}.submit();
			});

			/* remove/free watch entry */
			inotify_rm_watch(_fd, event->wd);
			_watched_nodes.remove(&elem);
			destroy(_heap, &elem);
		}
	};

	char buffer[EVENT_BUF_LEN] { 0 };
	while (true) {
		ssize_t const length { read(_fd, buffer, EVENT_BUF_LEN) };
		ssize_t       pos    { 0 };

		while (pos < length) {
			event = reinterpret_cast<struct inotify_event*>(&buffer[pos]);

			/* file modified? */
			if ((event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)) {
				Mutex::Guard guard { _watched_nodes_mutex };
				for_each(modify_fn);
			}

			/* watch descriptor no longer watched */
			else if (event->mask & IN_IGNORED) {
				Mutex::Guard guard { _watched_nodes_mutex };
				for_each(ignore_fn);
			}

			pos += EVENT_SIZE + event->len;
		}
	}
}


int Lx_fs::Notifier::add_watch(const char* path, Signal_context_capability cap)
{
	Mutex::Guard guard { _watched_nodes_mutex };

	if (!_watched(path)) {
		_add_to_watched(path);
	}

	return _add_cap(path, cap);
}


void Lx_fs::Notifier::remove_watch(char const *path, Signal_context_capability cap)
{
	Mutex::Guard guard { _watched_nodes_mutex };

	auto remove_fn = [&cap, &path, this] (Entry &elem) {
		if (elem.path == path) {

			elem.remove_capability(_heap, cap);
		}
	};

	auto *e { _notify_queue.first() };
	while (e != nullptr) {

		if (e->cap == cap) {
			auto *tmp { e };
			e = e->next();
			_notify_queue.remove(tmp);
		} else {
			e = e->next();
		}
	}

	for_each(remove_fn);
}
