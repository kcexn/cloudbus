# Cloudbus: Session-layer Control for Open Network Services
Cloudbus is a framework of distributed network proxies that support application developers and system administrators to transition from simple client-server deployments to 
highly-available application clusters. This transition to more complex distributed computing models typically becomes necessary as application computing requirements exceed what can 
be provided by a single server. Some example applications that need distributed computing include: data processing clusters for machine-learning, distributed 
databases, and distributed data pipelines. There are many open-source frameworks that provide turn-key distributed computing solutions e.g., Apache Storm, Apache 
CouchDB, Apache OpenWhisk, etc. Each of these frameworks imposes its own management and communication model for applications and services that depend on them. It is 
our experience that once applications require a sufficient degree of horizontal scale (often no more than a dozen nodes), these frameworks begin to exhibit signs of poor 
performance due to poor configuration, design limitations, or both. Common problems include:

- Poor usage of the available network bandwidth in the data pipeline between distributed services.
- Poor usage of transport protocol mechanisms like congestion control.
- Poor usage of tools like N-way fanout communication that are native to the distributed computing approach.
- Poor or limited implementations of rate-control and overload protection mechanisms.

Cloudbus aims to be a distributed I/O framework that addresses many of these problems and more at the session layer (OSI layer 5). Cloudbus will achieve this in three ways:

- Cloudbus communication modes that are native to distributed environments: one-to-one full-duplex, one-to-N half-duplex, and one-to-N full-duplex communication.
- Cloudbus exposes services to users through an autonomously managed dynamic DNS zone. (WIP)
- Cloudbus provides application agnostic rate-control and load-balancing mechanisms that dynamically accommodate for changing network conditions. (WIP)

## What does Cloudbus mean for me as an application developer?
Cloudbus's native support for one-to-N fanout communication makes it easy to implement robust distributed versions of common enterprise messaging patterns such as request/reply or 
publish/subscribe without the need for messaging libraries such as gRPC, or ZeroMQ. Simultaneously, since Cloudbus sits below the application layer, applications that already depend on 
these libraries can be migrated onto Cloudbus without any changes. Cloudbus will rely on dynamic DNS zones to configure how distributed application I/O should be routed within a 
network. This means that many decisions that affect performance like fan-out ratios for network I/O, can be decided by your system administrator. As an application developer, we hope 
that you will not need to make any changes to your existing software to take advantage of the benefits of Cloudbus.