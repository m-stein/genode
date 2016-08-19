
#ifndef _PROXY_ROLE_H_
#define _PROXY_ROLE_H_

#include <util/list.h>
#include <timer_session/connection.h>
#include <net/ipv4.h>
#include <net/tcp.h>
#include <net/udp.h>

namespace Net
{
	class Interface;
	class Tcp_proxy_role;
	class Udp_proxy_role;
}

class Net::Tcp_proxy_role : public Genode::List<Tcp_proxy_role>::Element
{
	private:

		using Signal_handler = Genode::Signal_handler<Tcp_proxy_role>;

		Genode::uint16_t const _client_port;
		Genode::uint16_t const _proxy_port;
		Ipv4_address const _client_ip;
		Ipv4_address const _proxy_ip;
		Interface & _client;
		Timer::Connection _timer;

		bool _client_fin = false;
		bool _other_fin = false;
		bool _client_fin_acked = false;
		bool _other_fin_acked = false;

		bool           _del = false;
		Signal_handler _del_timeout;
		unsigned const _del_timeout_us;
		void           _del_timeout_handle();

	public:

		Tcp_proxy_role(
			Genode::uint16_t client_port, Genode::uint16_t proxy_port,
			Ipv4_address client_ip, Ipv4_address proxy_ip,
			Interface & client, Genode::Entrypoint & ep,
			unsigned const rtt_sec);

		void print(Genode::Output & out) const;

		Genode::uint16_t client_port() const { return _client_port; }
		Genode::uint16_t proxy_port()  const { return _proxy_port; }
		Ipv4_address     client_ip()   const { return _client_ip; }
		Ipv4_address     proxy_ip()    const { return _proxy_ip; }
		Interface & client()      const { return _client; }
		bool             del()         const { return _del; }

		bool matches_client(
			Ipv4_address client_ip, Genode::uint16_t client_port);

		bool matches_proxy(
			Ipv4_address proxy_ip, Genode::uint16_t proxy_port);

		void tcp_packet(Ipv4_packet * const ip, Tcp_packet * const tcp);
};

class Net::Udp_proxy_role : public Genode::List<Udp_proxy_role>::Element
{
	private:

		using Signal_handler = Genode::Signal_handler<Udp_proxy_role>;

		Genode::uint16_t const _client_port;
		Genode::uint16_t const _proxy_port;
		Ipv4_address const _client_ip;
		Ipv4_address const _proxy_ip;
		Interface & _client;
		Timer::Connection _timer;

		bool           _del = false;
		Signal_handler _del_timeout;
		unsigned const _del_timeout_us;
		void           _del_timeout_handle();

	public:

		Udp_proxy_role(
			Genode::uint16_t client_port, Genode::uint16_t proxy_port,
			Ipv4_address client_ip, Ipv4_address proxy_ip,
			Interface & client, Genode::Entrypoint & ep,
			unsigned const rtt_sec);

		void print(Genode::Output & out) const;

		Genode::uint16_t client_port() const { return _client_port; }
		Genode::uint16_t proxy_port()  const { return _proxy_port; }
		Ipv4_address     client_ip()   const { return _client_ip; }
		Ipv4_address     proxy_ip()    const { return _proxy_ip; }
		Interface & client()      const { return _client; }
		bool             del()         const { return _del; }

		bool matches_client(
			Ipv4_address client_ip, Genode::uint16_t client_port);

		bool matches_proxy(
			Ipv4_address proxy_ip, Genode::uint16_t proxy_port);

		void udp_packet(Ipv4_packet * const ip, Udp_packet * const udp);
};

#endif /* _PROXY_ROLE_H_ */
