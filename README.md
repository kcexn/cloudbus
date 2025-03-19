# Cloudbus: Session-layer Control for Open Network Services
Cloudbus is a framework of distributed network proxies that support application developers and system administrators to 
transition from simple client-server deployments to highly-available application clusters. In addition to one-to-one client-server communication 
Cloudbus also supports the slightly novel one-to-N client-servers communication mode in both half and full-duplex modes. 

Cloudbus implements its own session-layer framing mechanism so that traffic from multiple clients can be multiplexed on a single transport. This can 
reduce latency for applications that require many outgoing requests. However, it means that traffic entering Cloudbus must be proxied 
by a Cloudbus `controller` and application servers must be reachable by a proxy called a Cloudbus `segment`. All traffic between `controller`'s and 
`segments`'s is managed completely internally by the Cloudbus framework.

## Installing Cloudbus
By default running:
```
./configure && make && make install
```
will install two binaries called `segment` and `controller` in `${prefix}/bin` and two example configuration files 
`segment.ini` and `controller.ini` in `${prefix}/etc/cloudbus`.

## Features and Limitations
Cloudbus currently supports:

- TCP and UNIX domain socket transport protocols.
- Half and full-duplex one-to-N communication between a controller and multiple segments.

Cloudbus currently does not support:

- DNS
- Abstract sockets, UDP, and SCTP transports.

Additionally:

- Each segment process can currently only bind to one socket at a time. This means that segments can not support multiple back-end services.

Adding these features and addressing these limitations is currently WIP.