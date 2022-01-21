/*
 * \brief  Wireguard component
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>

/* lx-kit includes */
#include <lx_kit/env.h>

/* lx-emul includes */
#include <lx_emul/init.h>

/* lx-user includes */
#include <lx_user/io.h>

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <base64.h>
#include <net.h>
#include <config.h>

using namespace Genode;

namespace Wireguard { class  Main; }


/*
 * Dummy device list implementation replacement for lx_kit
 */
Lx_kit::Device_list::Device_list(Entrypoint           &,
                                 Heap                 &,
                                 Platform::Connection &platform)
: _platform { platform } { }


class Wireguard::Main : private Entrypoint::Io_progress_handler
{
	private:

		Env                    &_env;
		Heap                    _heap            { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config_rom      { _env, "config"        };
		Signal_handler<Main>    _config_handler  { _env.ep(), *this,
		                                           &Main::_handle_config };
		Io_signal_handler<Main> _signal_handler  { _env.ep(), *this,
		                                           &Main::_handle_signal };
		Config_model            _config_model    { _heap                 };

		Vpn       _vpn       { _env, _heap, _signal_handler,
		                       _config_rom.xml().attribute_value("vpn", Ipv4_address_prefix {}) };
		Local_net _local_net { _env, _heap, _signal_handler,
		                       _config_rom.xml().attribute_value("local", Ipv4_address_prefix {}) };

		void _handle_signal()
		{
			lx_user_handle_io();
			Lx_kit::env().scheduler.schedule();
		}

		void _handle_config() { _config_rom.update(); }

	public:

		Main(Env &env)
		:
			_env(env)
		{
			Lx_kit::initialize(_env);

			_config_rom.sigh(_config_handler);
			_handle_config();

			env.ep().register_io_progress_handler(*this);

			/* trigger signal handling once after construction */
			Signal_transmitter(_signal_handler).submit();
		}

		/**
		 * Entrypoint::Io_progress_handler
		 */
		void handle_io_progress() override
		{
			_local_net.notify_peer();
			_vpn.notify_peer();
		}

		void update(genode_wg_config_callbacks & callbacks)
		{
			_config_model.update(callbacks, _config_rom.xml());
		}

		void net_receive(genode_wg_net_receive_t rcv_callback)
		{
			_local_net.for_each_rx_packet(rcv_callback);
			_vpn.for_each_rx_packet(rcv_callback);
		}

		bool net_send(void * buf, size_t buf_size, bool to_local)
		{
			return to_local ? _local_net.tx_one_packet(buf, buf_size)
			                : _vpn.tx_one_packet(buf, buf_size);
		}

		void send_wg_prot_at_nic_connection(
			genode_wg_u8_t const *wg_prot_base,
			genode_wg_u64_t       wg_prot_size,
			genode_wg_u16_t       udp_src_port_big_endian,
			genode_wg_u16_t       udp_dst_port_big_endian,
			genode_wg_u32_t       ipv4_src_addr_big_endian,
			genode_wg_u32_t       ipv4_dst_addr_big_endian,
			genode_wg_u8_t        ipv4_dscp_ecn,
			genode_wg_u8_t        ipv4_ttl);
};


void Wireguard::Main::send_wg_prot_at_nic_connection(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_u64_t       wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t       ipv4_src_addr_big_endian,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl)
{
	_vpn.send_wg_prot(
		wg_prot_base,
		wg_prot_size,
		udp_src_port_big_endian,
		udp_dst_port_big_endian,
		ipv4_src_addr_big_endian,
		ipv4_dst_addr_big_endian,
		ipv4_dscp_ecn,
		ipv4_ttl);
}


static Wireguard::Main & main_object(Genode::Env & env)
{
	static Wireguard::Main main { env };
	return main;
}


extern "C" void
genode_wg_update_config(struct genode_wg_config_callbacks * callbacks)
{
	main_object(Lx_kit::env().env).update(*callbacks);
};


extern "C" void
genode_wg_net_receive(genode_wg_net_receive_t rcv_callback)
{
	main_object(Lx_kit::env().env).net_receive(rcv_callback);
}


extern "C" int
genode_wg_net_send(void * buf, unsigned long buf_size, int up)
{
	return (main_object(Lx_kit::env().env).net_send(buf, buf_size, up))
		? 0 : -1;
}


void genode_wg_send_wg_prot_at_nic_connection(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_u64_t       wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t       ipv4_src_addr_big_endian,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl)
{
	main_object(Lx_kit::env().env).send_wg_prot_at_nic_connection(
		wg_prot_base,
		wg_prot_size,
		udp_src_port_big_endian,
		udp_dst_port_big_endian,
		ipv4_src_addr_big_endian,
		ipv4_dst_addr_big_endian,
		ipv4_dscp_ecn,
		ipv4_ttl);
}


void Component::construct(Env &env)
{
	main_object(env);

	/*
	 * Main needs to be constructed before startin Linux code,
	 * because of genode_wg_* calls
	 */
	lx_emul_start_kernel(nullptr);
}
