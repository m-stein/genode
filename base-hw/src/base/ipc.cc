/*
 * \brief  Implementation of the Genode IPC API
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>

/* Kernel includes */
#include <kernel/syscalls.h>
#include <kernel/log.h>

using namespace Genode;

enum
{
	/* Size of the information that is marshalled into an IPC message
	 * to tell the callee wich RPC object is targeted */
	RPC_OBJECT_ID_SIZE = sizeof(umword_t),

	/* The RPC framework marshalls a return value into reply messages to
	 * deliver exceptions, wich occured during the RPC call to the caller */
	RPC_RETURN_VALUE_SIZE = sizeof(umword_t),
};


/***************
 ** Utilities **
 ***************/

/**
 * Translate byte size 's' to size in words
 */
static unsigned long size_in_words(unsigned long const s)
{
	return (s + sizeof(unsigned long) - 1) / sizeof(unsigned long);
}


/**
 * Copy message payload to message buffer
 */
static void copy_utcb_to_msgbuf(Msgbuf_base * const receive_buffer,
                                unsigned long const message_size)
{
	/* Log data that is received via IPC */
	enum { VERBOSE = 0 };

	/* Get pointers and message attributes */
	Native_utcb * const   utcb   = Thread_base::myself()->utcb();
	unsigned long * const msgbuf = (unsigned long *)receive_buffer->buf;
	unsigned long const   message_wsize = size_in_words(message_size);

	/* Assertions, we don't want to print in here or even throw exceptions */
	if (message_wsize > size_in_words(utcb->size())) while(1) ;
	if (VERBOSE) kernel_log() << "Thread " << thread_get_my_native_id()
	                          << " recvs ";

	/* Fill message buffer with message */
	for (unsigned i=0; i < message_wsize; i++)
	{
		msgbuf[i] = *utcb->word(i);
		if (VERBOSE) kernel_log() << *utcb->word(i) << " ";
	}
	if (VERBOSE) kernel_log() << "\n";
}


/**
 * Copy message payload to UTCB
 */
static void copy_msgbuf_to_utcb(Msgbuf_base * const send_buffer,
                                unsigned long const message_size,
                                unsigned long const local_name)
{
	/* Log data that is send via IPC */
	enum { VERBOSE = 0 };

	/* Get pointers and message attributes */
	Native_utcb * const   utcb   = Thread_base::myself()->utcb();
	unsigned long * const msgbuf = (unsigned long *)send_buffer->buf;
	unsigned long const   message_wsize = size_in_words(message_size);

	/* Assertions, we don't want to print in here or even throw exceptions */
	if (message_wsize > size_in_words(utcb->size())) while(1) ;

	/* Address message to an object that the targeted thread knows */
	*utcb->word(0) = local_name;
	if (VERBOSE) kernel_log() << "Thread " << thread_get_my_native_id()
	                          << " sends " << *utcb->word(0) << " ";

	/* Write message payload */
	for (unsigned long i = 1; i < message_wsize; i++)
	{
		*utcb->word(i) = msgbuf[i];
		if (VERBOSE) kernel_log() << *utcb->word(i) << " ";
	}
	if (VERBOSE) kernel_log() << "\n";
}


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = RPC_OBJECT_ID_SIZE;
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait()
{
	Kernel::pause_thread();
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg) :
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	Native_capability(Genode::thread_get_my_native_id(), 0),
	_rcv_msg(rcv_msg),
	_rcv_cs(-1)
{
	_read_offset = RPC_OBJECT_ID_SIZE;
}


Ipc_istream::~Ipc_istream()
{ }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	/* Send request and receive reply */
	copy_msgbuf_to_utcb(_snd_msg, _write_offset,
	                    Ipc_ostream::_dst.local_name());
	copy_utcb_to_msgbuf(_rcv_msg,
	                    Kernel::request_and_wait(Ipc_ostream::_dst.dst(),
	                                             _write_offset));

	/* Reset unmarshaller */
	_write_offset = _read_offset = RPC_OBJECT_ID_SIZE;
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


/****************
 ** Ipc_server **
 ****************/

Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg),
	_reply_needed(false)
{ }


void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	/* Leave space for RPC method return value */
	_write_offset = RPC_OBJECT_ID_SIZE + RPC_RETURN_VALUE_SIZE;

	_read_offset = RPC_OBJECT_ID_SIZE;
}


void Ipc_server::_wait()
{
	/* Receive next request */
	copy_utcb_to_msgbuf(_rcv_msg, Kernel::wait_for_request());

	/* Update server state */
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Unexpected call\n";
	while(1) ;
}


void Ipc_server::_reply_wait()
{
	if (!_reply_needed) {
		_wait();
		return;
	}

	/* Send reply and receive next request */
	copy_msgbuf_to_utcb(_snd_msg, _write_offset,
	                    Ipc_ostream::_dst.local_name());
	copy_utcb_to_msgbuf(_rcv_msg, Kernel::reply_and_wait(_write_offset));

	/* Update server state */
	_prepare_next_reply_wait();
}

