
#ifndef _PROXY_ROLE_H_
#define _PROXY_ROLE_H_

#include <util/list.h>
#include <timer_session/connection.h>
#include <net/ipv4.h>
#include <net/tcp.h>

namespace Net
{
	class Packet_handler;
	class Proxy_role;
}

class Net::Proxy_role : public Genode::List<Proxy_role>::Element
{
	private:

		using Signal_handler = Genode::Signal_handler<Proxy_role>;

		Genode::uint16_t const _client_port;
		Genode::uint16_t const _proxy_port;
		Ipv4_address const _client_ip;
		Ipv4_address const _proxy_ip;
		Packet_handler & _client;
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

		Proxy_role(
			Genode::uint16_t client_port, Genode::uint16_t proxy_port,
			Ipv4_address client_ip, Ipv4_address proxy_ip,
			Packet_handler & client, Genode::Entrypoint & ep,
			unsigned const rtt_sec);

		Genode::uint16_t client_port() const { return _client_port; }
		Genode::uint16_t proxy_port()  const { return _proxy_port; }
		Ipv4_address     client_ip()   const { return _client_ip; }
		Ipv4_address     proxy_ip()    const { return _proxy_ip; }
		Packet_handler & client()      const { return _client; }
		bool             del()         const { return _del; }

		bool matches_client(
			Ipv4_address client_ip, Genode::uint16_t client_port);

		bool matches_proxy(
			Ipv4_address proxy_ip, Genode::uint16_t proxy_port);

		void tcp_packet(Ipv4_packet * const ip, Tcp_packet * const tcp);
};

#endif /* _PROXY_ROLE_H_ */
