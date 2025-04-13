![builds](https://github.com/kcexn/cloudbus/actions/workflows/c-cpp.yml/badge.svg)

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
    LATEST=0.0.4 && \
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
To run cloudbus you just execute the binary from your shell by typing:
```
$ segment
```
or 
```
$ controller
```
To run Cloudbus as a daemon on systems using systemd, create a symlink to the provided systemd service units.
Then enable and start the cloudbus components using systemctl. You will need to do this 
as the root user:
```
# SYSTEMD_PATH=/etc/systemd/system
# PREFIX=/usr/local
# ln -s ${PREFIX}/etc/cloudbus/systemd/controller.service ${SYSTEMD_PATH}/controller.service
# ln -s ${PREFIX}/etc/cloudbus/systemd/segment.service ${SYSTEMD_PATH}/segment.service
# systemctl enable controller segment
# systemctl start controller segment
```
If you have changed the PREFIX at `configure` time, then PREFIX needs to be set to the path you have configured.

_Cloudbus currently does not support daemonization on systems that do not use systemd._

## Configuring Cloudbus
Cloudbus is designed to be configured in terms of user-defined services. Each component of cloudbus will bind 
to a local address on the host for each service that is defined. Each component of cloudbus will have one 
or more backends defined for each service. Cloudbus connects to all backends defined on a service using one-to-N 
fanout communication channels. To configure round-robin load balancing, backends need to be defined using 
a hostname and round-robin load-balancing is applied to the addresses supplied by DNS. 

Configuration files for the cloudbus controller and segment will be installed into `${prefix}/etc/cloudbus` and 
are called `controller.ini` and `segment.ini` respectively. Controllers and segments use the same configuration 
format:
```
[Cloudbus]

[<ServiceName>]
bind=<PROTOCOL>://<IP ADDRESS>:<PORT>
backend=<PROTOCOL>://(<IP ADDRESS> | <HOSTNAME>):<PORT>
backend=<PROTOCOL>://(<IP ADDRESS> | <HOSTNAME>):<PORT>
mode=(half_duplex | full_duplex)

[<ServiceName>]
bind=<PROTOCOL>://<IP ADDRESS>:<PORT>
backend=<PROTOCOL>://(<IP ADDRESS> | <HOSTNAME>):<PORT>
...

```
Where \<ServiceName\> is an arbitrarily chosen human readable name to assign 
to each service, \<PROTOCOL\> can be one of:
- tcp
- unix

\<IP ADDRESS\> is either an IPv4 or an IPv6 address enclosed in square brackets
e.g.:
- IPv4 address: 127.0.0.1
- IPv6 address: [::1]

\<HOSTNAME\> is a domain name and \<PORT\> is an ephemeral port number. 
Backends for each service can take either a \<HOSTNAME\> as an argument or an 
\<IP ADDRESS\>. 

Services defined on Cloudbus controllers can be assigned multiple backend addresses, but each backend address 
must be a Cloudbus segment. Controllers are designed to connect to all backend segments defined on each service in 
a one-to-N fashion. By default, controllers will make this connection in half-duplex mode, and any segment that 
replies with data to the controller will latch the session into one-to-one full-duplex mode. For example, a 
half-duplex session may have the following behavior:
1. Controller sends data to N backend segments.
2. backend 1 replies to the controller with data.
3. The controller forwards this data back to the user.
4. The controller then closes the connection to backends 2 through N so that no further data can be 
sent to or received from them.
5. The session is now in one-to-one full-duplex mode.

To configure round-robin load balancing on a Cloudbus controller, a single backend should be assigned to the 
service using a hostname, and multiple addresses should be assigned to the hostname in DNS. Round-robin 
load-balancing will be applied to the addresses returned by DNS. To configure Cloudbus controllers for one-to-N 
full-duplex communication the service mode should be set to "full_duplex" (no quotation marks, case insensitive). 
In full-duplex mode, all data sent by the client is duplicated across all the backends defined in the service, 
and all data sent by the backends is multiplexed back to the client. Currently, Cloudbus doesn't provide any 
synchronization mechanisms for this kind of communication so care should be taken to make messages from each 
backend distinguishable at the application layer.

Each service on a Cloudbus segment can only be assigned one backend. For more granular load balancing, round-robin 
load-balancing based on DNS hostname resolution can be applied, or a layer 4 load-balancer should be used.

### Example Configuration:
#### Simple Service with One Backend:
**Controller configuration:**
```
[Cloudbus]

[Simple Service]
bind=unix:///var/run/simple_controller.sock
backend=tcp://192.168.1.2:8082
```
**Segment configuration:**
```
[Cloudbus]

[Simple Service]
bind=tcp://0.0.0.0:8082
backend=unix:///var/run/simple_service.sock
```

### Uninstalling Cloudbus
In the directory where you have unpacked the distribution files run (as root):
```
# make uninstall
```
If you have symlinked the provided systemd service units and are running cloudbus as a 
daemon then you will also need to run (as root):
```
# systemctl stop segment controller
# systemctl disable segment controller
```

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
