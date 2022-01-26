/*
 * \brief  Wireguard component configuration
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
#include <util/list_model.h>

/* app/wireguard includes */
#include <ipv4_address_prefix.h>

using namespace Genode;

namespace Wireguard { struct Config_model; }


struct Wireguard::Config_model
{
	using Key_base64 = String<WG_KEY_LEN_BASE64>;

	struct Peer : List_model<Peer>::Element
	{
		Key_base64             public_key;
		Ipv4_address           ip;
		uint16_t               port;
		Ipv4_address_prefix    allowed_ip;

		Peer(Key_base64 key, Ipv4_address ip, uint16_t port, Ipv4_address_prefix allowed_ip)
		: public_key(key), ip(ip), port(port), allowed_ip(allowed_ip) { }
	};

	struct Peer_update_policy : List_model<Peer>::Update_policy
	{
		Genode::Allocator          & alloc;
		genode_wg_config_callbacks & callbacks;
		uint16_t                     port;

		Peer_update_policy(Allocator                  & a,
		                   genode_wg_config_callbacks & c,
		                   uint16_t                     p)
		: alloc(a), callbacks(c), port(p) {}

		void destroy_element(Element & e)
		{
			callbacks.remove_peer(port, e.ip.addr, e.port);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			uint8_t key_buf[WG_KEY_LEN];
			Ipv4_address        ip = node.attribute_value("ip", Ipv4_address { });
			uint16_t             p = node.attribute_value("port", (uint16_t)0U );
			Key_base64           k = node.attribute_value("public_key", Key_base64());
			Ipv4_address_prefix  a = node.attribute_value("allowed_ip", Ipv4_address_prefix());

			if (!k.valid() || !key_from_base64(key_buf, k.string()))
				error("Invalid public key!");

			if (!a.valid())
				error("Invalid allowed ip!");

			callbacks.add_peer(
				port, ip.addr, p, key_buf, a.address.addr, a.prefix);

			return *(new (alloc) Element(k, ip, p, a));
		}

		void update_element(Element &, Xml_node) { }

		static bool element_matches_xml_node(Element const & e, Xml_node node)
		{
			Ipv4_address ip = node.attribute_value("ip", Ipv4_address { });
			uint16_t      p = node.attribute_value("port", (uint16_t)0U );
			Key_base64    k = node.attribute_value("public_key", Key_base64());

			return (ip == e.ip) && (p == e.port) && (k == e.public_key);
		}

		static bool node_is_element(Xml_node node) {
			return node.has_type("peer"); }
	};

	struct Config : List_model<Config>::Element
	{
		Key_base64       private_key;
		uint16_t         port;
		List_model<Peer> peers {};

		Config(Key_base64 key, uint16_t port) : private_key(key), port(port) {}
	};

	struct Config_update_policy : List_model<Config>::Update_policy
	{
		Genode::Allocator          & alloc;
		genode_wg_config_callbacks & callbacks;

		Config_update_policy(Allocator & a, genode_wg_config_callbacks & c)
		: alloc(a), callbacks(c) {}

		void destroy_element(Element & e)
		{
			Peer_update_policy policy(alloc, callbacks, e.port);
			e.peers.destroy_all_elements(policy);
			callbacks.remove_device(e.port);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			uint8_t    key_buf[WG_KEY_LEN];
			Key_base64 key  = node.attribute_value("private_key", Key_base64());
			Ipv4_address ip = node.attribute_value("listen_ip", Ipv4_address { });
			uint16_t   port = node.attribute_value("listen_port", (uint16_t)0U);

			if (!key.valid() || !key_from_base64(key_buf, key.string()))
				error("Invalid private key!");

			callbacks.add_device(ip.to_uint32_big_endian(), port, key_buf);
			return *(new (alloc) Element(key, port));
		}

		void update_element(Element & e, Xml_node node)
		{
			Peer_update_policy policy(alloc, callbacks, e.port);
			e.peers.update_from_xml(policy, node);
		}

		static bool element_matches_xml_node(Element const & e, Xml_node node)
		{
			return e.port == node.attribute_value("listen_port", (uint16_t)0U);
		}

		static bool node_is_element(Xml_node node) {
			return node.has_type("iface"); }
	};

	Allocator        & alloc;
	List_model<Config> config {};

	Config_model(Allocator & alloc) : alloc(alloc) {}

	void update(genode_wg_config_callbacks & callbacks, Xml_node node)
	{
		Config_update_policy policy(alloc, callbacks);
		config.update_from_xml(policy, node);
	}
};

