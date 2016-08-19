
#include <proxy_role.h>
#include <interface.h>

using namespace Net;
using namespace Genode;

static bool verbose = 0;

void Tcp_proxy_role::_del_timeout_handle()
{
	_del = true;
	if (verbose) { log("Deprecate TCP proxy ", *this); }

}


Tcp_proxy_role::Tcp_proxy_role
(
	uint16_t client_port, uint16_t proxy_port, Ipv4_address client_ip,
	Ipv4_address proxy_ip, Interface & client, Genode::Entrypoint &ep,
	unsigned const rtt_sec)
:
	_client_port(client_port), _proxy_port(proxy_port), _client_ip(client_ip),
	_proxy_ip(proxy_ip), _client(client),
	_del_timeout(ep, *this, &Tcp_proxy_role::_del_timeout_handle),
	_del_timeout_us(rtt_sec * 2 * 1000 * 1000)
{
	if (verbose) { log("Create TCP proxy ", *this); }

}


bool Tcp_proxy_role::matches_client
(
	Ipv4_address client_ip, uint16_t client_port)
{
	return client_ip == _client_ip && client_port == _client_port;
}


bool Tcp_proxy_role::matches_proxy
(
	Ipv4_address proxy_ip, uint16_t proxy_port)
{
	return proxy_ip == _proxy_ip && proxy_port == _proxy_port;
}


void Tcp_proxy_role::tcp_packet(Ipv4_packet * const ip, Tcp_packet * const tcp)
{
	/* find out which side sent the packet */
	bool from_client;
	if (tcp->src_port() == _client_port) { from_client = true; }
	else { from_client = false; }

	/* Remember FIN packets and which side sent them */
	if (tcp->fin()) {
		if (from_client) { _client_fin = true; }
		else { _other_fin  = true; }
	}
	/* look for packets that ACK a previous FIN and remember those ACKs */
	if (tcp->ack()) {
		if (from_client  && _other_fin)  { _other_fin_acked = true; }
		if (!from_client && _client_fin) { _client_fin_acked = true; }

		/* if both sides sent a FIN and got ACKed, init delayed destruction */
		if (_other_fin_acked && _client_fin_acked) {
			_timer.sigh(_del_timeout);
			_timer.trigger_once(_del_timeout_us);
		}
	}
}

void Udp_proxy_role::_del_timeout_handle()
{
	_del = true;
	if (verbose) { log("Deprecate UDP proxy ", *this); }

}


void Udp_proxy_role::print(Output & out) const
{
	Genode::print(out, _client_ip, ":", _client_port, " -> ",
	              _proxy_ip, ":", _proxy_port);
}

void Tcp_proxy_role::print(Output & out) const
{
	Genode::print(out, _client_ip, ":", _client_port, " -> ",
	              _proxy_ip,  ":", _proxy_port);
}


Udp_proxy_role::Udp_proxy_role
(
	uint16_t client_port, uint16_t proxy_port, Ipv4_address client_ip,
	Ipv4_address proxy_ip, Interface & client, Genode::Entrypoint &ep,
	unsigned const rtt_sec)
:
	_client_port(client_port), _proxy_port(proxy_port), _client_ip(client_ip),
	_proxy_ip(proxy_ip), _client(client),
	_del_timeout(ep, *this, &Udp_proxy_role::_del_timeout_handle),
	_del_timeout_us(rtt_sec * 2 * 1000 * 1000)
{
	if (verbose) { log("Create UDP proxy ", *this); }
	_timer.sigh(_del_timeout);
	_timer.trigger_once(_del_timeout_us);
}


bool Udp_proxy_role::matches_client
(
	Ipv4_address client_ip, uint16_t client_port)
{
	return client_ip == _client_ip && client_port == _client_port;
}


bool Udp_proxy_role::matches_proxy
(
	Ipv4_address proxy_ip, uint16_t proxy_port)
{
	return proxy_ip == _proxy_ip && proxy_port == _proxy_port;
}


void Udp_proxy_role::udp_packet(Ipv4_packet * const ip, Udp_packet * const udp)
{
	_timer.trigger_once(_del_timeout_us);
}
