# CHANGELOG

## Implemented Functionality

- TCP and UDP port scanning over both IPv4 and IPv6
- Raw socket SYN scans for TCP (IPv4/IPv6)
- UDP scanning using ICMP (IPv4) and ICMPv6 (IPv6) unreachable responses
- Interface selection and hostname resolution
- Timeout handling for all scan types
- Packet capture via libpcap for IPv6 TCP responses (SYN-ACK, RST)
- Proper checksum computation for all packet types

## Known Limitations

- **TCP/UDP over IPv6**: When a port is open, the scanner may wait for the full timeout period before marking it as open. This is due to the design of the passive packet capture mechanism (libpcap for TCP, raw socket + `select()` for UDP), which cannot immediately detect a positive response unless it's a known rejection (e.g., RST or ICMP unreachable).
