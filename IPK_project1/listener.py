#!/usr/bin/env python3
from scapy.all import *

# Configure these for your environment:
LISTEN_IP   = "100.69.166.88"  # The IP on which we'll "listen" for UDP
UNREACH_PORT = 9999            # The UDP port we'll fake as "closed"

def send_icmp_unreachable(original_pkt):
    """
    Crafts and sends an ICMP Destination Unreachable (type=3, code=3)
    in response to the original UDP packet.
    """
    # Parse original IP/UDP headers
    ip_layer = original_pkt[IP]
    udp_layer = original_pkt[UDP]

    # Build a new IP layer (reverse src/dst)
    # The ICMP error should come 'from' the IP we are faking as closed
    ip_reply = IP(
        src=ip_layer.dst,
        dst=ip_layer.src
    )

    # Build ICMP layer: type=3, code=3 => "Port Unreachable"
    icmp_layer = ICMP(type=3, code=3)

    # Per RFC, the ICMP payload should include the original IP header + first 8 bytes of original payload
    # Scapy automatically does this if we do icmp_layer / original_pkt[IP]
    # but let's be explicit with slicing:
    icmp_payload = raw(ip_layer)[:ip_layer.ihl*4 + 8]  # IP header + first 8 bytes of UDP header

    # Construct final packet
    reply_pkt = ip_reply / icmp_layer / icmp_payload

    # Send it
    send(reply_pkt, verbose=False)
    print(f"Sent ICMP port unreachable to {ip_layer.src}:{udp_layer.sport} for UDP dport={udp_layer.dport}")

def handle_packet(pkt):
    """
    Callback for sniff(). If a UDP packet arrives at LISTEN_IP:UNREACH_PORT,
    send an ICMP type 3 code 3 back.
    """
    if IP in pkt and UDP in pkt:
        ip_layer = pkt[IP]
        udp_layer = pkt[UDP]
        if ip_layer.dst == LISTEN_IP and udp_layer.dport == UNREACH_PORT:
            print(f"Received UDP packet from {ip_layer.src}:{udp_layer.sport} to {ip_layer.dst}:{udp_layer.dport}")
            send_icmp_unreachable(pkt)

if __name__ == "__main__":
    # You might need to disable firewall or allow raw sockets, run as root, etc.
    print(f"Listening for UDP packets to {LISTEN_IP}:{UNREACH_PORT} and sending ICMP port unreachable...")
    # Build a BPF filter to capture only our target traffic
    # e.g., "udp and dst host 192.168.1.100 and dst port 9999"
    bpf_filter = f"udp and dst host {LISTEN_IP} and dst port {UNREACH_PORT}"

    sniff(filter=bpf_filter, prn=handle_packet, store=False, iface="lo")