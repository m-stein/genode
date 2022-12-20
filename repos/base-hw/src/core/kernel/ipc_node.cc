/*
 * \brief   Backend for end points of synchronous interprocess communication
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* core includes */
#include <kernel/ipc_node.h>
#include <kernel/thread.h>

using namespace Kernel;


void Ipc_node::_receive_from(Ipc_node &node)
{
	_thread.ipc_copy_msg(node._thread);
	_caller = &node;
	_state  = READY;
}


void Ipc_node::_cancel_send()
{
	if (_out_node) {
		if (_out_node->_caller == this) {
			_out_node->_caller = nullptr;
		} else {
			_out_node->_in_queue.remove(_queue_item);
		}
		_out_node = nullptr;
	}
	if (_out_sending()) {
		_thread.ipc_send_request_failed();
		_state = READY;
	}
}


bool Ipc_node::_helping() const
{
	return _state == OUT_SEND_HELPING;
}


bool Ipc_node::can_send_request() const
{
	return _state == READY;
}


void Ipc_node::send_request(Ipc_node &node, bool help)
{
	if (_state == OUT_SEND_HELPING) {
		_state = OUT_SEND;
	}
	if (node._in_waiting()) {
		node._receive_from(*this);
		node._thread.ipc_await_request_succeeded();
	} else {
		node._in_queue.enqueue(_queue_item);
	}
	_out_node = &node;
	_state = help ? OUT_SEND_HELPING : OUT_SEND;
}


Thread &Ipc_node::helping_sink()
{
	return _helping() ? _out_node->helping_sink() : _thread;
}


bool Ipc_node::can_await_request() const
{
	return _state == READY;
}


void Ipc_node::await_request()
{
	_state = IN_WAIT;
	_in_queue.dequeue([&] (Queue_item &item) {
		_receive_from(item.object());
	});
}


void Ipc_node::send_reply()
{
	if (_state == READY && _caller) {
		Ipc_node &node { *_caller };
		node._thread.ipc_copy_msg(_thread);
		node._state = READY;
		node._thread.ipc_send_request_succeeded();
		_caller = nullptr;
	}
}


void Ipc_node::cancel_waiting()
{
	if (_out_sending()) {
		_cancel_send();
	}
	if (_in_waiting()) {
		_state = READY;
		_thread.ipc_await_request_failed();
	}
}


Ipc_node::Ipc_node(Thread &thread)
:
	_thread(thread)
{ }


Ipc_node::~Ipc_node()
{
	_state = DESTRUCT;

	_cancel_send();

	if (_caller) {
		_caller->_cancel_send();
	}
	_in_queue.dequeue_all([] (Queue_item &item) {
		item.object()._cancel_send();
	});
}
