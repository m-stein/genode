/*
 * \brief  Test for the NIC loop-back service
 * \author Norman Feske
 * \date   2009-11-14
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

namespace Local {

	using namespace Genode;
	struct Construct_destruct_test;
	struct Main;
}


struct Local::Construct_destruct_test
{
	enum { MAX_CNT  = 300 };
	enum { PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
	enum { BUF_SIZE = 1000 * PKT_SIZE };

	Env                            &_env;
	Allocator                      &_alloc;
	Signal_context_capability      &_completed_sigh;
	Nic::Packet_allocator           _pkt_alloc { &_alloc };
	Constructible<Nic::Connection>  _nic       { };

	Construct_destruct_test(Env                       &env,
	                        Allocator                 &alloc,
	                        Signal_context_capability  completed_sigh)
	:
		_env            { env },
		_alloc          { alloc },
		_completed_sigh { completed_sigh }
	{
		for (unsigned cnt = 0; cnt < MAX_CNT; cnt++) {
			_nic.construct(_env, &_pkt_alloc, BUF_SIZE, BUF_SIZE);
			_nic.destruct();
			log(cnt);
		}
		Signal_transmitter(_completed_sigh).submit();
	}
};


struct Local::Main
{
	Env                                    &_env;
	Heap                                    _heap   { &_env.ram(), &_env.rm() };
	Constructible<Construct_destruct_test>  _test_1 { };

	Signal_handler<Main> _test_completed_handler {
		_env.ep(), *this, &Main::_handle_test_completed };

	void _handle_test_completed()
	{
		if (_test_1.constructed()) {
			_test_1.destruct();
			log("--- finished NIC stress test ---");
			_env.parent().exit(0);
		}
	}

	Main(Env &env) : _env(env)
	{
		log("--- NIC stress test ---");
		_test_1.construct(_env, _heap, _test_completed_handler);
	}
};


void Component::construct(Genode::Env &env) { static Local::Main main(env); }
