/*
 * \brief  Test the reachability of a host on an IP network
 * \author Martin Stein
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <base/attached_rom_dataspace.h>

using namespace Net;
using namespace Genode;

#define ASSERT(condition) \
	do { \
		if (!(condition)) { \
			Genode::error(__FILE__, ":", __LINE__, ": ", " assertion \"", #condition, "\" failed "); \
			Genode::sleep_forever(); \
		} \
	} while (false)

#define ASSERT_NEVER_REACHED \
	do { \
		Genode::error(__FILE__, ":", __LINE__, ": ", " should have never been reached"); \
		Genode::sleep_forever(); \
	} while (false)


struct Pcap_file_header
{
	static constexpr uint32_t MAGIC_NUMBER = 0xA1B2C3D4;

	uint32_t magic_number;
	uint32_t unused[5];

	bool valid() const { return magic_number == MAGIC_NUMBER; }
};


struct Pcap_packet_record
{
	uint32_t unused_0[2];
	uint32_t captured_pkt_len;
	uint32_t original_pkt_len;
};


struct Main
{
	Env &env;
	Attached_rom_dataspace pcap_rom { env, "capture.pcap" };
	Const_byte_range_ptr pcap_range { pcap_rom.local_addr<char>(), pcap_rom.size() };

	Main(Env &env);
};


Main::Main(Env &env) : env(env)
{
	static constexpr size_t BUF_SIZE = 1024;
	char buf[BUF_SIZE];
	unsigned long num_errors = 0;
	unsigned long num_packets = 0;

	size_t offset = 0;
	ASSERT(pcap_range.num_bytes >= sizeof(Pcap_file_header));
	Pcap_file_header const &header = *(Pcap_file_header const *)pcap_range.start;
	offset += sizeof(Pcap_file_header);
	ASSERT(header.valid());

	while(1) {
		if (pcap_range.num_bytes - offset < sizeof(Pcap_packet_record))
			break;

		Pcap_packet_record const &record = *(Pcap_packet_record const *)(pcap_range.start + offset);
		offset += sizeof(Pcap_packet_record);
		if (!record.captured_pkt_len)
			break;

		ASSERT(record.captured_pkt_len == record.original_pkt_len);
		log("check packet #", num_packets+1," captured_len=", record.captured_pkt_len);

		ASSERT(record.captured_pkt_len <= BUF_SIZE);
		memcpy(buf, pcap_range.start + offset, record.captured_pkt_len);
		offset += record.captured_pkt_len;
		Size_guard size_guard(record.captured_pkt_len);
		Ethernet_frame &eth = Ethernet_frame::cast_from(buf, size_guard);

		ASSERT(eth.type() == Ethernet_frame::Type::IPV4);
		Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
		uint16_t pcap_ip_checksum = ip.checksum();
		if (ip.checksum_error()) {
			error("validating pcap ip checksum failed");
			num_errors++;
		}
		ip.update_checksum();
		if (ip.checksum() != pcap_ip_checksum) {
			error("calculating ip checksum failed (got ", Hex(ip.checksum())," expected ", Hex(pcap_ip_checksum), ",)");
			num_errors++;
		}
		num_packets++;
	}
	log("checked ", num_packets, " packet", num_packets == 1 ? "" : "s");
	env.parent().exit(num_errors ? -1 : 0);
}


void Component::construct(Env &env) { static Main main(env); }
