/*
 * \brief  Stress test for low level NIC interactions
 * \author Martin Stein
 * \date   2019-03-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

namespace Local {

	using namespace Genode;
	struct Construct_destruct_test;
	struct Main;
}


struct Local::Construct_destruct_test
{
	enum { DEFAULT_NR_OF_ROUNDS  = 10 };
	enum { DEFAULT_NR_OF_SESSIONS = 10 };
	enum { PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
	enum { BUF_SIZE = 100 * PKT_SIZE };

	Env                            &_env;
	Allocator                      &_alloc;
	Signal_context_capability      &_completed_sigh;
	Xml_node                 const &_config;
	Nic::Packet_allocator           _pkt_alloc     { &_alloc };
	bool                     const  _config_exists { _config.has_sub_node("construct_destruct") };

	unsigned long const _nr_of_rounds {
		_config_exists ?
			_config.sub_node("construct_destruct").
				attribute_value("nr_of_rounds",
				                (unsigned long)DEFAULT_NR_OF_ROUNDS) :
			(unsigned long)DEFAULT_NR_OF_ROUNDS };

	unsigned long const _nr_of_sessions {
		_config_exists ?
			_config.sub_node("construct_destruct").
				attribute_value("nr_of_sessions",
				                (unsigned long)DEFAULT_NR_OF_SESSIONS) :
				(unsigned long)DEFAULT_NR_OF_SESSIONS };

	Construct_destruct_test(Env                       &env,
	                        Allocator                 &alloc,
	                        Signal_context_capability  completed_sigh,
	                        Xml_node            const &config)
	:
		_env            { env },
		_alloc          { alloc },
		_completed_sigh { completed_sigh },
		_config         { config }
	{
		if (_nr_of_rounds && _nr_of_sessions) {

			using Nic_slot = Constructible<Nic::Connection>;

			size_t           const ram_size { _nr_of_sessions * sizeof(Nic_slot) };
			Attached_ram_dataspace ram_ds   { _env.ram(), _env.rm(), ram_size };
			Nic_slot        *const nic      { ram_ds.local_addr<Nic_slot>() };

			for (unsigned round = 0; round < _nr_of_rounds; round++) {
				for (unsigned idx = 0; idx < _nr_of_sessions; idx++) {
					nic[idx].construct(_env, &_pkt_alloc, BUF_SIZE, BUF_SIZE);
					log("round ", round + 1, " nic ", idx + 1, " mac ",
					    nic[idx]->mac_address());
				}
				for (unsigned idx = 0; idx < _nr_of_sessions; idx++) {
					nic[idx].destruct();
				}
			}
		}
		Signal_transmitter(_completed_sigh).submit();
	}
};


struct Local::Main
{
	Env                                    &_env;
	Heap                                    _heap       { &_env.ram(), &_env.rm() };
	Attached_rom_dataspace                  _config_rom { _env, "config" };
	Xml_node                          const _config     { _config_rom.xml() };
	Constructible<Construct_destruct_test>  _test_1     { };

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
		_test_1.construct(_env, _heap, _test_completed_handler, _config);
	}
};


void Component::construct(Genode::Env &env) { static Local::Main main(env); }
