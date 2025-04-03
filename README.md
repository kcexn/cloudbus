# Cloudbus: Session-layer Control for Network Services
Cloudbus is a framework of distributed network proxies that aims to support application 
developers and system administrators to transition from simple client-server application 
architectures to distributed application clusters, for instance, micro-service 
based architectures. Cloudbus achieves this by providing useful IPC tools that make 
the complicated parts of networking, distributed messaging, service exposure, and 
service discovery easy (or easier). Cloudbus supports one-to-one client-server 
communication, as well as one-to-N client-servers communication in both half and 
full-duplex modes of operation. Cloudbus implements asynchronous DNS resolution 
through the use of the [c-ares](https://c-ares.org/) library, and implements round-robin 
load-balancing for hostnames with multiple A and/or AAAA records.

Cloudbus implements its own session-layer framing mechanism so that traffic from 
multiple clients *may* be multiplexed on a single transport. This can reduce latency for 
applications that require many outgoing requests. However, it means that traffic entering 
Cloudbus must be proxied by a Cloudbus `controller` and application servers must be 
reachable by a proxy called a Cloudbus `segment`. All traffic between `controller`'s and 
`segments`'s is managed completely internally by the Cloudbus framework.

Cloudbus is **not** a substitute or replacement for application and layer 4 
load-balancers like [HAProxy](https://www.haproxy.org/) or 
[Linux Virtual Server](http://www.linuxvirtualserver.org/). Cloudbus' primary goal 
is to handle traffic that is **internal** to your distributed application architecture 
and make it easier for developers and administrators to both configure and reason about 
the networking and communication aspects of your application design.

Cloudbus is also **not** a substitute for messaging frameworks like 
[ZeroMQ](https://zeromq.org/) as Cloudbus' session-layer framing applies a data *stream* 
abstraction rather than an atomic *message* abstraction to application traffic. This 
narrowing of scope means that instead of focusing on delivering features that are 
common in messaging frameworks like automatic retrying, fault-recovery strategies, and 
message delivery guarantees, Cloudbus can focus just on making the hard parts of 
networked IPC easier, e.g, service related configuration, DNS resolution, transport and 
network-layer optimizations etc.

Your distributed application architecture might have load-balancers and a messaging 
framework *in addition* to Cloudbus for internal network configuration.

Developer's looking to use Cloudbus to build or prototype a distributed application 
simply need to configure their service backends in Cloudbus segments, and configure their 
service frontends in Cloudbus controllers (see 
[Configuring Cloudbus](#configuring-cloudbus)). Service backends can be defined using a 
protocol (currently TCP, or Unix domain sockets), a port number, and either an 
IP address or a domain name. In the future, Cloudbus plans to support both 
NAPTR and SRV record resolution so that distributed services can be configured using 
an arbitrary URN and dynamically resolved using NAPTR substitution rules.

## Build and Install
Cloudbus has only been tested on Linux. I believe that it will work on any POSIX compliant 
system. It will not work on Windows (yet).

### Installing Dependencies
Cloudbus depends on [c-ares](https://c-ares.org/) version 1.18.1 or greater. You can build 
c-ares from source by downloading a release directly from the c-ares 
[Github repository](https://github.com/c-ares/c-ares/releases) or you can install the 
c-ares library provided by your Linux distributions package manager. If you are installing 
c-ares using a package manager, make sure you also install the development headers 
otherwise Cloudbus will not build.

#### Installing c-ares on Debian:
```
$ sudo apt-get install libc-ares-dev
```

### Download, Build, and Install Cloudbus
Download the [latest release](https://github.com/kcexn/cloudbus/releases/latest)
and extract it into a local directory.
```
$ URL=https://github.com/kcexn/cloudbus/releases/download && \
    LATEST=0.0.2 && \
    wget ${URL}/v${LATEST}/cloudbus-${LATEST}.tar.gz -O - | tar -zxvf - && \
    cd cloudbus-${LATEST}
```

Then running
```
$ ./configure && make && make install
```
will install two binaries called `segment` and `controller` in `${prefix}/bin` and 
two example configuration files `segment.ini` and `controller.ini` in 
`${prefix}/etc/cloudbus`. `${prefix}` defaults to `/usr/local`.

Depending on what `${prefix}` has been set to, you may need to run `make install` as the 
root user, e.g.:
```
$ ./configure && make
$ su
# make install
```
A variety of build and installation options can be set by the configure script. See:
```
$ ./configure --help
```
for more.

## Running Cloudbus
Currently, to run cloudbus you just execute the binary from your shell by typing:
```
$ segment
```
or 
```
$ controller
```
Cloudbus doesn't automatically run as a daemon yet (this will be implemented eventually). You can run either of these binaries in the background by relying on your shells 
job control mechanism to background their execution, i.e:
```
$ segment &
$ controller &
```
To run either of these at startup you will need to write a systemd service unit. 
**Cloudbus is not production ready**, so I do not recommend you do this.

## Configuring Cloudbus
Configuration files for the cloudbus controller and segment will be installed into `${prefix}/etc/cloudbus` and are called `controller.ini` and `segment.ini` respectively.
Controllers and segments use the same configuration format:
```
[Cloudbus]

[<ServiceName>]
bind=<ADDRESS>
server=<ADDRESS>
server=<ADDRESS>
mode=(half duplex | full duplex)

[<ServiceName>]
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

## Features and Limitations
Cloudbus currently supports:

- TCP and UNIX domain socket transport protocols.
- Half and full-duplex one-to-N communication between a controller and multiple segments.

Cloudbus currently does not support:
- Abstract sockets, UDP, and SCTP transports.
- [TIPC](https://docs.kernel.org/networking/tipc.html) (Transparent Inter Process 
        Communication Protocol)
- Automatic daemonization.

Cloudbus currently supports an implementation of DNS for hostname resolution *only*.
i.e.:
```
<PROTOCOL>://<FQDN>:<PORTNUM>
```
At some stage, there is a plan to implement NAPTR and SRV record resolution.

Adding these features and addressing these limitations is currently WIP.