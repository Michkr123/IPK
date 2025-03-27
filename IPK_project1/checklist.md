General Requirements

- [X] Scan Target:
    - [X] Scan a specified hostname or IP address (support for both IPv4 and IPv6).
    - [X] If multiple IP addresses are returned from DNS, scan each one.

- [x] Interface Selection:
    - [x] Accept a network interface parameter (-i or --interface).
    - [x] If no interface is specified (or only the flag is given), list active interfaces.

- [X] Timeout Configuration:
    - [x] Accept a timeout parameter (-w or --wait) in milliseconds.
    - [x] Default timeout value should be 5000 ms if not specified.
    - [x] Termination:
        Allow the program to be terminated at any time with Ctrl + C.

TCP Scanning

- [x] Packet Sending:
    - [x] Send TCP SYN packets (do not complete the full 3-way handshake).
    - [x] Use raw sockets to craft and send SYN packets.

- [x] Response Handling:
    - [x] If a SYN/ACK response is received, mark the port as open.
    - [x] If an RST response is received, mark the port as closed.
    - [x] If no response is received after the timeout, retransmit the SYN once.
    - [x] If still no response, mark the port as filtered.

UDP Scanning

- [x] Packet Sending:
    - [x] Send UDP packets to the specified ports
- [X] Response Handling:
    - [X] If an ICMP message of type 3, code 3 (port unreachable) is received, mark the port as closed.
    - [X] If no response is received, consider the port as open.

Implementation & Execution

- [X] Sockets and Packet Handling:
    - [x] Use sockets for sending/receiving packets.
    - [X] Optionally, use the libpcap library to capture responses if needed.

Command-Line Interface (CLI)

- [X] Help Message:
    - [X] Implement a -h/--help option that writes usage instructions to stdout and terminates.

- [X] Interface Parameter:
    - [X] Implement -i or --interface to specify the network interface.
    - [X] If only this parameter is provided without other required values, print a list of active interfaces.

- [X] Port Parameters:
    - [X] Implement -t/--pt for specifying TCP port ranges (e.g., --pt 22,80,81 or --pt 22-80).
    - [X] Implement -u/--pu for specifying UDP port ranges.

- [X] Timeout Parameter:
    - [X] Implement -w/--wait to specify the timeout (e.g., -w 3000).
    - [X] Target Specification:
    - [X] Accept either a hostname (e.g., www.vutbr.cz) or an IP address (IPv4 or IPv6) as the target.

Output Format

- [X] Result Lines:
    - [X] Each output line must include (separated by spaces):
    ```
    <scanned IP> <port number> <protocol> <port state>
    ```

- [X] Port States:
    - [X] Allowed states are open, closed, and filtered.
    - [X] Example Output:
        127.0.0.1 22 tcp open
        127.0.0.1 21 udp closed

Documentation

- [X] Manual/Documentation:
    - [X] Provide a user manual or documentation that explains:
        - How to compile and run the program.
        - Detailed usage instructions and examples.

- [X] Assignment Documentation:
    - [X] Include any additional notes or explanations that would help a tester or user understand your implementation.