/*
 * \brief  File-system node
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

#ifndef _WATCH_H_
#define _WATCH_H_

/* Genode includes */
#include <base/id_space.h>
#include <file_system/node.h>
#include <file_system/open_node.h>
#include <os/path.h>
#include <os/vfs.h>

/* local includes */
#include "node.h"
#include "notifier.h"
#include "open_node.h"


namespace Lx_fs {

	using namespace File_system;
	using Vfs::Vfs_watch_handle;

	class Watch_node;
}


class Lx_fs::Watch_node final : public Lx_fs::Node,
                                public Vfs::Watch_response_handler
{
	public:

		using Packet_descriptor = File_system::Packet_descriptor;
		using Fs_open_node      = File_system::Open_node<Lx_fs::Watch_node>;

		struct Watch_node_response_handler : Genode::Interface
		{
			virtual void handle_watch_node_response(Watch_node &) = 0;
		};

	private:

		using Signal_handler = Genode::Signal_handler<Watch_node>;

		/*
		 * Noncopyable
		 */
		Watch_node(Watch_node const &) = delete;
		Watch_node &operator = (Watch_node const &) = delete;

		Env                          &_env;
		Watch_node_response_handler  &_watch_node_response_handler;
		Notifier                     &_notifier;
		int                           _notify_fd { -1 };
		Signal_handler                _signal_handler { _env.ep(), *this, &Watch_node::watch_response };
		Packet_descriptor             _acked_packet { };
		Fs_open_node                 *_open_node { nullptr };

		unsigned long _inode(char const *path)
		{
			struct stat s { };
			int ret { stat(path, &s) };
			if (ret == -1)
				throw Lookup_failed();

			return s.st_ino;
		}

	public:

		Watch_node(Env                         &env,
		           char const                  *path,
		           Watch_node_response_handler &watch_node_response_handler,
		           Notifier                    &notifier)
		:
			Node { _inode(path) },
			_env { env },
			_watch_node_response_handler { watch_node_response_handler },
			_notifier { notifier }
		{
			name(path);

			_notify_fd = notifier.add_watch(path, _signal_handler);
			if (_notify_fd < 0) {
				throw Lookup_failed { };
			}
		}

		virtual ~Watch_node()
		{
			_notifier.remove_watch(name(), _signal_handler);
		}

		/*******************************************
		 ** Vfs::Watch_response_handler interface **
		 *******************************************/

		void watch_response() override
		{
			mark_as_updated();
			_acked_packet = Packet_descriptor { Packet_descriptor { },
			                                    Node_handle { _open_node->id().value },
			                                    Packet_descriptor::CONTENT_CHANGED,
			                                    0, 0 };
			_acked_packet.succeeded(true);

			_watch_node_response_handler.handle_watch_node_response(*this);
		}

		void update_modification_time(Timestamp const) override { }
		size_t read(char *, size_t, seek_off_t) override { return 0; }
		size_t write(char const *, size_t, seek_off_t) override { return 0; }
		Status status() override { return Status { }; }

		Packet_descriptor &acked_packet() { return _acked_packet; }

		void open_node(Fs_open_node *open_node) { _open_node = open_node; }
		Fs_open_node *open_node() { return _open_node; }
};


#endif /* _WATCH_H_ */
