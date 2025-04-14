# Developer Notes
## Dependencies:

* Cloudbus depends on c-ares version 1.18.1 or greater, releases can be downloaded 
  from the c-ares [Github repository](https://github.com/c-ares/c-ares/releases). 
  Alternatively, you can install the libc-ares binary and associated development 
  headers from your distro's package manager, e.g., on Debian:
  ```
  $ sudo apt-get install libc-ares-dev
  ```

## Configure and Build:

* Cloudbus is built using autotools. To generate the `configure` script run:
  ```
  $ autoreconf -i
  ```

* By default, Cloudbus components are designed to pick up configuration from 
  `${PREFIX}/etc/cloudbus`. To make components search the working directory for 
  configuration configure the `CONFDIR` compiler flag to be explicitly unset. i.e.:
  ```
  ./configure CXXFLAGS='-UCONFDIR'
  ```

* This can be combined with VPATH builds to nicely separate out any development 
  related configuration from configuration that will be bundled in the release 
  tarballs. i.e.:
  ```
  $ mkdir build && cd build
  $ ../configure CXXFLAGS='-UCONFDIR'
  ```
