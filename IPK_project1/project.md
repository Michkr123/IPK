# Project 2 - OMEGA: L4 Scanner

## Assignment
1. Create a simple TCP and UDP network L4 scanner. The program will scan the specified hostname or IP address(es) (plural IP addresses in the case of multiple answers to DNS query) and ports. It will output to stdout port statuses (open, filtered, closed) (7 pts.) 
2. Create relevant manual/documentation for the project (3 pts.)

## Specification
The application scans the selected ports of device (translated onto one or more IPv4/IPv6 addresses) on a given network interface. 

Packets/Frames should be sent using sockets. If needed, you can eavesdrop on the responses using the libpcap library.

The program can be terminated at any given moment with `Ctrl + C` sequence.

Scanning should be done and return results as fast as possible. During development and testing, try scanning only the computers you own or manage.

#### TCP scanning
Sends only SYN packets. It does not perform a complete 3-way-handshake. If an RST response is received - the port is marked as *closed*. If no response is received from the scanned port after a given time interval, it must be verified with another packet and only then the port is marked as *filtered*. If a service is running on the port, the port is marked as *open*. See RFC 793 for more information.

#### UDP scanning
With UDP scanning, you can think of a given computer responding with an ICMP message of type 3, code 3 (port unreachable) when the port is *closed*. Consider the other ports as *open*.

### Execution
```
./ipk-l4-scan [-i interface | --interface interface] [--pu port-ranges | --pt port-ranges | -u port-ranges | -t port-ranges] {-w timeout} [domain-name | ip-address]
```

where:

* `-i eth0` (just one interface to scan through) or `--interface`. If this parameter is not specified (and any other parameters as well), or if only `-i/--interface` is specified without a value (and any other parameters are unspecified), a list of active interfaces is printed (additional information beyond the interface list is welcome but not required).
* `-t` or `--pt`, `-u` or `--pu` port-ranges - scanned tcp/udp ports, allowed entry e.g., `--pt 22` or `--pu 1-65535` or `--pt 22,23,24`. The --pu and --pt arguments can be specified separately, i.e. they do not have to occur both at once if the user wants only TCP or only UDP scanning
* `-w 3000` or `--wait 3000`, is the timeout in milliseconds to wait for a response for a single port scan. This parameter is optional, in its absence the value 5000 (i.e., five seconds) is used.
* either `domain-name` or `ip-address`, which either fully qualified domain name or IP address of scanned device.
* All arguments can be in any order.

### Execution Examples
```
./ipk-l4-scan --interface eth0 -u 53,67 2001:67c:1220:809::93e5:917
./ipk-l4-scan -i eth0 -w 1000 -t 80,443,8080 www.vutbr.cz
```

### Functionality Illustration
```
./ipk-l4-scan -i eth0 --pt 21,22,143 --pu 53,67 localhost

Interesting ports on localhost (127.0.0.1):
PORT STATE
21/tcp closed
22/tcp open
143/tcp filtered
53/udp closed
67/udp open
```

Illustrated command line output can be customised to provide relevant information in a more structured way.

## Bibliography
* RFC 793: Transmission Control Protocol, 1981. Online. Request for Comments. Internet Engineering Task Force. [Accessed 17 February 2025]. 
* RFC 791: Internet Protocol, 1981. Online. Request for Comments. Internet Engineering Task Force. [Accessed 17 February 2025]. 
* RFC 768: User Datagram Protocol, 1980. Online. Request for Comments. Internet Engineering Task Force. [Accessed 17 February 2025]. 
* Nmap: The Art of Port Scanning. Online. Available from: https://nmap.org/nmap_doc.html#port_unreach [Accessed 17 February 2025]. 
* TCP SYN (Stealth) Scan (-sS) | Nmap Network Scanning. Online. Available from: https://nmap.org/book/synscan.html [Accessed 17 February 2025]. 
* Port scanner, 2024. Wikipedia. Online. Available from: https://en.wikipedia.org/w/index.php?title=Port_scanner&oldid=1225200572 [Accessed 17 February 2025]. 
* DEERING, Steve E. and HINDEN, Bob, 2017. RFC 8200: Internet Protocol, Version 6 (IPv6) Specification. Online. Request for Comments. Internet Engineering Task Force. [Accessed 17 February 2025]. 
* GILLIGAN, Robert E., BOUND, Jim, THOMSON, Susan and STEVENS, W. Richard, 1999. RFC 2553: Basic Socket Interface Extensions for IPv6. Online. Request for Comments. Internet Engineering Task Force. [Accessed 17 February 2025]. 
* SATRAPA, Pavel. IPv6: internetový protokol verze 6. CZ. NIC, 2019. ISBN: 978-80-88168-43-0