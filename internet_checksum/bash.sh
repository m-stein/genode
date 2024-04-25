#/bin/bash

# generate traffic from packet patterns in config file
sudo trafgen -r -i trafgen.cfg -n 1 -o capture.pcap

# display generated trafic in detail
tshark -V -r capture.pcap -o ip.check_checksum:TRUE -o tcp.check_checksum:TRUE -o udp.check_checksum:TRUE

# display only packets with bad checksums
#tshark -r capture.pcap -Y "ip.checksum_bad || tcp.checksum_bad.expert || udp.checksum.bad"
