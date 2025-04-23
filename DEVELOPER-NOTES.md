# Developer Notes
## Dependencies:

* Cloudbus depends on c-ares v1.18.1 or greater. Releases can be downloaded 
    from the c-ares [repository](https://github.com/c-ares/c-ares/releases). 
* Cloudbus also depends on pcre2 v10.42 or greaer. Releases can be downloaded 
    from the pcre2 [repository](https://github.com/PCRE2Project/pcre2/releases).

Alternatively, dependencies and their development headers can be installed from 
distro's package manager, e.g., on Debian:
```
$ sudo apt-get install libc-ares-dev libpcre2-dev
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
  $ ./configure CXXFLAGS='-UCONFDIR'
  ```

* Development tests are disabled by default so that software tests do not need to be 
  included in the distributed tarball. Enable tests at configure time with:
  ```
  $ ./configure CXXFLAGS='-UCONFDIR' --enable-tests
  ```

* This can be combined with VPATH builds to nicely separate out any development 
  related configuration from configuration that will be bundled in the release 
  tarballs. i.e.:
  ```
  $ mkdir build && cd build
  $ ../configure CXXFLAGS='-UCONFDIR' --enable-tests
  ```
