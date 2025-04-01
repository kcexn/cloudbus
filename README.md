# Cloudbus: Session-layer Control for Network Services
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
- Abstract sockets, UDP, and SCTP transports.

Cloudbus currently supports an implementation DNS for hostname resolution *only*.
i.e.:
```
<PROTOCOL>://<FQDN>:<PORTNUM>
```
At some stage, there is a plan to implement NAPTR and SRV record resolution.

Adding these features and addressing these limitations is currently WIP.

## Configuring Cloudbus

Configuration files for the cloudbus controller and segment will be installed into `${prefix}/etc/cloudbus` and are called `controller.ini` and `segment.ini` respectively.
Controllers and segments use the same configuration format:
```
[Cloudbus]

[ServiceName]
bind=<ADDRESS>
server=<ADDRESS>
server=<ADDRESS>
mode=(half duplex | full duplex)

[ServiceName]
bind=<ADDRESS>
server=<ADDRESS>
...

```

Each service on a cloudbus segment can currently only be assigned one backend server. For more granular load balancing on a segment, you will need to use an L4 load-balancer.

Each service on a cloudbus controller supports more than one assigned backend server. By default, controllers will simultaneously connect to every backend server in the list in half-duplex mode.
For full duplex mode, the service mode should be set to the string "full duplex" (no quotation marks, case insensitive).

Services with only one backend server defined default to one-to-one full duplex communication. Servers that send data back to the client on a half-duplex one-to-N connection will latch the session 
into one-to-one full-duplex mode (i.e., client sends data to N servers in half duplex mode -> server N sends data back to client (perhaps an error) -> bus closes remaining N-1 half-duplex connections -> session 
is now in one-to-one full duplex mode).
