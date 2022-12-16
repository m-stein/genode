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


void Ipc_node::_receive(Ipc_node & from)
{
	_thread.ipc_copy_msg(from._thread);
	_in.state = In::REPLY;
}


void Ipc_node::_cancel_receive(Ipc_node & from)
{
	/*
	 * Check whether this node already actively replies
	 * the message that needs to be canceled
	 */
	if (_in.state == In::REPLY)
		_in.queue.head([&] (Queue_item &item) {
			if (&item == &from._queue_item) _in.state = In::REPLY_NO_SENDER; });

	_in.queue.remove(from._queue_item);
}


void Ipc_node::_cancel_send()
{
	if (_out.node) {
		_out.node->_cancel_receive(*this);
		_out.node = nullptr;
	}

	if (_out.sending()) {
		_thread.ipc_send_request_failed();
		_out.state = Out::READY;
	}
}


bool Ipc_node::_helping() const
{
	return _out.state == Out::SEND_HELPING && _out.node;
}


bool Ipc_node::can_send_request() const
{
	return _out.state == Out::READY && !_in.waiting();
}


void Ipc_node::send_request(Ipc_node & to, bool help)
{
	to._in.queue.enqueue(_queue_item);

	if (to._in.waiting()) {
		to._receive(*this);
		to._thread.ipc_await_request_succeeded();
	}

	_out.node  = &to;
	_out.state = help ? Out::SEND_HELPING : Out::SEND;
}


Thread &Ipc_node::helping_sink()
{
	return _helping() ? _out.node->helping_sink() : _thread;
}


bool Ipc_node::can_await_request() const
{
	return _in.state == In::READY;
}


void Ipc_node::await_request()
{
	_in.state = In::WAIT;

	_in.queue.head([&] (Queue_item &item) {
		_receive(item.object()); });
}


void Ipc_node::send_reply()
{
	if (_in.state == In::REPLY)
		_in.queue.dequeue([&] (Queue_item &item)
		{
			Ipc_node & from = item.object();
			from._thread.ipc_copy_msg(_thread);
			from._out.node  = nullptr;
			from._out.state = Out::READY;
			from._thread.ipc_send_request_succeeded();
		});

	_in.state = In::READY;
}


void Ipc_node::cancel_waiting()
{
	if (_out.sending()) _cancel_send();

	if (_in.waiting()) {
		_in.state = In::READY;
		_thread.ipc_await_request_failed();
	}
}


Ipc_node::Ipc_node(Thread & thread) : _thread(thread) { }


Ipc_node::~Ipc_node()
{
	_in.state  = In::DESTRUCT;
	_out.state = Out::DESTRUCT;

	_cancel_send();

	_in.queue.for_each([&] (Queue_item &item) {
		item.object()._cancel_send(); });
}
