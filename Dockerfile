# Argument for the application version, with a default value.
ARG prefix=/usr/local
ARG version=0.4.0

# ---- Builder Stage ----
# This stage will download dependencies, source code, and build the application.
FROM debian:stable-slim AS builder
ARG prefix
ARG version

# Set DEBIAN_FRONTEND to noninteractive to prevent apt-get from prompting for input.
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies:
# - build-essential: For compiling C/C++ code (gcc, make, etc.)
# - libc-ares-dev: Development files for c-ares library
# - libpcre2-dev: Development files for PCRE2 library
# - coreutils: For `nproc` utility to determine number of CPU cores for parallel make
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        libc-ares-dev \
        libpcre2-dev \
        coreutils && \
    rm -rf /var/lib/apt/lists/*

# Set the working directory for the build process.
WORKDIR /build_src

COPY cloudbus-${version}.tar.gz ./
RUN tar -zxf cloudbus-${version}.tar.gz && \
    cd cloudbus-${version} && \
    ./configure --prefix=${prefix} CXXFLAGS='-flto' && \
    make -j$(nproc)

# ---- Runtime Stage ----
# This stage will create the final, minimal image for running the application.
FROM debian:stable-slim
ARG prefix
ARG version
ARG segment_conf=conf/segment.ini.orig
ARG controller_conf=conf/controller.ini.orig
ARG component=controller

# Set DEBIAN_FRONTEND to noninteractive.
ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies.
# Based on the -dev packages in the builder stage, these are the likely runtime libraries.
# You can verify these by using `ldd` on the compiled binary in the builder stage.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libc-ares2 \
        libpcre2-8-0 && \
    rm -rf /var/lib/apt/lists/*

# Create a system group and user with no login shell for running the application.
RUN groupadd --system appgroup && \
    useradd --no-log-init --system --gid appgroup appuser && \
    mkdir -p /var/run/cloudbus && \
    chown appuser:appgroup /var/run/cloudbus && \
    chmod 0770 /var/run/cloudbus

# Copy the compiled application binary from the builder stage.
COPY --from=builder /build_src/cloudbus-${version}/controller \
    /build_src/cloudbus-${version}/segment \
    ${prefix}/bin/
COPY ${controller_conf} ${prefix}/etc/cloudbus/controller.ini
COPY ${segment_conf} ${prefix}/etc/cloudbus/segment.ini

# Ensure the binary is executable.
RUN chmod +x ${prefix}/bin/controller && \
    chmod +x ${prefix}/bin/segment

WORKDIR /home/appuser
USER appuser

ENV COMPONENT=${component}
ENTRYPOINT ["sh", "-c", "exec $COMPONENT \"$@\""]
