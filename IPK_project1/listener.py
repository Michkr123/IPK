#!/usr/bin/env python3
from scapy.all import *

# This function will be called for each packet captured that matches our filter.
def udp_listener(packet):
    # Check that the packet has an IP and UDP layer.
    if IP in packet and UDP in packet:
        # Check if the destination UDP port is 12345 (the port we are "simulating" closed)
        if packet[UDP].dport == 12345:
            print(f"Received UDP packet from {packet[IP].src}:{packet[UDP].sport}")
            # Create an IP layer for the reply: swap source and destination.
            ip_reply = IP(src=packet[IP].dst, dst=packet[IP].src)
            # Create an ICMP layer with type 3 (Destination Unreachable) and code 3 (Port Unreachable).
            icmp_reply = ICMP(type=3, code=3)
            # According to RFC, include the IP header and the first 8 bytes of the original UDP packet.
            # We'll include the first 28 bytes (20 bytes of IP header + 8 bytes of UDP header).
            original_data = bytes(packet[IP])[:28]
            # Build the complete ICMP packet.
            icmp_packet = ip_reply / icmp_reply / Raw(load=original_data)
            send(icmp_packet, verbose=False)
            print(f"Sent ICMP unreachable to {packet[IP].src}")

if __name__ == "__main__":
    # Change "12345" to your desired UDP port to simulate.
    # You can also add an interface parameter in sniff() if needed (e.g., iface="lo").
    print("Listening for UDP packets on port 12345...")
    sniff(filter="udp and dst port 12345", prn=udp_listener)
