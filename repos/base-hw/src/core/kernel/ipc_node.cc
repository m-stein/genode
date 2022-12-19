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
	_state  = INACTIVE;
}


void Ipc_node::_cancel_send()
{
	if (_callee) {
		if (_callee->_caller == this) {
			_callee->_caller = nullptr;
		} else {
			_callee->_request_queue.remove(_request_queue_item);
		}
		_callee = nullptr;
	}
	if (_out_sending()) {
		_thread.ipc_send_request_failed();
		_state = INACTIVE;
	}
}


bool Ipc_node::_helping() const
{
	return (_state == AWAIT_REPLY) && _help;
}


bool Ipc_node::can_send_request() const
{
	return _state == INACTIVE;
}


void Ipc_node::send_request(Ipc_node &callee, bool help)
{
	_state    = AWAIT_REPLY;
	_callee   = &callee;
	_help     = false;

	if (_callee->_in_waiting()) {
		_callee->_receive_from(*this);
		_callee->_thread.ipc_await_request_succeeded();
	} else {
		_callee->_request_queue.enqueue(_request_queue_item);
	}
	_help = help;
}


Thread &Ipc_node::helping_sink()
{
	return _helping() ? _callee->helping_sink() : _thread;
}


bool Ipc_node::can_await_request() const
{
	return _state == INACTIVE;
}


void Ipc_node::await_request()
{
	_state = AWAIT_REQUEST;
	_request_queue.dequeue([&] (Queue_item &item) {
		_receive_from(item.object());
	});
}


void Ipc_node::send_reply()
{
	if (_state == INACTIVE && _caller) {
		_caller->_thread.ipc_copy_msg(_thread);
		_caller->_state = INACTIVE;
		_caller->_thread.ipc_send_request_succeeded();
		_caller = nullptr;
	}
}


void Ipc_node::cancel_waiting()
{
	if (_out_sending()) {
		_cancel_send();
	}
	if (_in_waiting()) {
		_state = INACTIVE;
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
		if (_caller->_callee) {
			_caller->_callee = nullptr;
			_caller->_state  = INACTIVE;
			_caller->_thread.ipc_send_request_failed();
			_caller = nullptr;
		}
	}
	_request_queue.dequeue_all([] (Queue_item &item) {
		Ipc_node &node { item.object() };
		if (node._callee) {
			node._callee = nullptr;
			node._state  = INACTIVE;
			node._thread.ipc_send_request_failed();
		}
	});
}

