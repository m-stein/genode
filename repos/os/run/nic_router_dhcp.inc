if {[have_spec foc] || [have_spec linux] || [have_spec rpi3] ||
    [expr [have_spec imx53] && [have_spec trustzone]]} {

	puts "Run script is not supported on this platform."
	exit 0
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg]

build {

	init
	server/dynamic_rom
	test/nic_router_dhcp
	server/nic_router
	server/report_rom
}

append config {

<config>

	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	</parent-provides>

	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="200"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>

	<start name="drivers" caps="1000" managing_system="yes">
		<resource name="RAM" quantum="32M"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config">
				<parent label="drivers.config"/>
			</service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<provides> <service name="Nic"/> </provides>
	</start>

	<start name="dynamic_rom">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="nic_router_1.config">
				<inline>

					<config link_state_min_dwell_ms="200">

						<policy label="nic_router_2 -> " domain="downlink"/>
						<uplink                          domain="uplink"/>

						<domain name="uplink"/>
						<domain name="downlink" interface="10.2.3.1/24">

							<dhcp-server ip_first="10.2.3.2"
										 ip_last="10.2.3.2">

								<dns-server ip="1.2.3.4"/>
								<dns-server ip="2.3.4.5"/>
								<dns-server ip="3.4.5.6"/>

							</dhcp-server>

						</domain>

					</config>

				</inline>
				<sleep milliseconds="3000"/>
				<inline>

					<config link_state_min_dwell_ms="200">

						<policy label="nic_router_2 -> " domain="downlink"/>
						<uplink                          domain="uplink"/>

						<domain name="uplink"/>
						<domain name="downlink" interface="10.2.3.1/24">

							<dhcp-server ip_first="10.2.3.2"
										 ip_last="10.2.3.2">

								<dns-server ip="4.5.6.7"/>
								<dns-server ip="5.6.7.8"/>

							</dhcp-server>

						</domain>

					</config>

				</inline>
				<sleep milliseconds="3000"/>
				<inline>

					<config link_state_min_dwell_ms="200">

						<policy label="nic_router_2 -> " domain="downlink"/>
						<uplink                          domain="uplink"/>

						<domain name="uplink"/>
						<domain name="downlink" interface="10.2.3.1/24">

							<dhcp-server ip_first="10.2.3.2"
							             ip_last="10.2.3.2">

								<dns-server ip="6.7.8.9"/>

							</dhcp-server>

						</domain>

					</config>

				</inline>
				<sleep milliseconds="3000"/>
				<inline>

					<config link_state_min_dwell_ms="200">

						<policy label="nic_router_2 -> " domain="downlink"/>
						<uplink                          domain="uplink"/>

						<domain name="uplink"/>
						<domain name="downlink" interface="10.2.3.1/24">

							<dhcp-server ip_first="10.2.3.2"
							             ip_last="10.2.3.2">

							</dhcp-server>

						</domain>

					</config>

				</inline>
				<sleep milliseconds="3000"/>

			</rom>
		</config>
	</start>

	<start name="nic_router_1">
		<binary name="nic_router"/>
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Nic"/></provides>
		<route>
			<service name="ROM" label="config">
				<child name="dynamic_rom" label="nic_router_1.config"/>
			</service>
			<service name="Nic"> <child name="drivers"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic_router_2">
		<binary name="nic_router"/>
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Nic"/></provides>}

append_if [expr ![nic_router_2_managed]] config {

		<config verbose_packets="no"
		        link_state_min_dwell_ms="200">

			<report bytes="no"
					stats="no"
					quota="no"
					config="yes"
					config_triggers="yes"
					interval_sec="100"/>

			<policy label="test_client -> " domain="downlink"/>
			<uplink                         domain="uplink"/>

			<domain name="uplink"/>
			<domain name="downlink" interface="10.0.3.1/24">

				<dhcp-server ip_first="10.0.3.2"
				             ip_last="10.0.3.2"
				             dns_server_from="uplink"/>

			</domain>

		</config>}

append config {

		<route>
			<service name="ROM" label="config">
				<child name="report_rom"/>
			</service>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="Nic"> <child name="nic_router_1"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config verbose="no">
			<policy label="test_manager -> router_state"
			        report="nic_router_2 -> state"/>

			<policy label="nic_router_2 -> config"
			        report="test_manager -> router_config"/>
		</config>
	</start>

	<start name="test_client">
		<binary name="test-nic_router_dhcp-client"/>
		<resource name="RAM" quantum="10M"/>
		<config verbose="no"/>
		<route>
			<service name="Nic"> <child name="nic_router_2"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>}

append_if [nic_router_2_managed] config {

	<start name="test_manager">
		<binary name="test-nic_router_dhcp-manager"/>
		<resource name="RAM" quantum="1M"/>
		<route>
			<service name="ROM" label="router_state">
				<child name="report_rom"/>
			</service>
			<service name="Report"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>}

append config {

</config>}

install_config $config

set boot_modules {

	init
	dynamic_rom
	nic_router
	test-nic_router_dhcp-client
	report_rom
}

lappend_if [nic_router_2_managed] boot_modules test-nic_router_dhcp-manager

build_boot_image $boot_modules

append qemu_args " -nographic "
append_qemu_nic_args

append done_string ".*DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"
append done_string ".*DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 4.5.6.7.*\n"
append done_string ".*  DNS server #2: 5.6.7.8.*\n"
append done_string ".*DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 6.7.8.9.*\n"
append done_string ".*DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"

run_genode_until $done_string 30
