# Define the source files for the library
set(CBUTILS_SOURCES
    config/config.cpp
    connector/connector_timerqueue.cpp
    connector/connectors.cpp
    formats/xmsg.cpp
    interfaces/interfaces.cpp
    io/sockbuf.cpp
    io/poller.cpp
    messages/messages.cpp
    node/node.cpp
    manager/manager.cpp
    dns/dns.cpp
    metrics/metrics.cpp
    options/options.cpp
    logging/logging.cpp
)

# Define the header files associated with the library
# These are useful for IDE integration and can be used with target_sources()
# While not strictly compiled into a static library, they define its interface and implementation details.
set(CBUTILS_HEADERS
    config/config.hpp
    connector/connector_timerqueue.hpp
    connector/connectors.hpp
    formats/xmsg.hpp
    interfaces/interfaces.hpp
    io/io.hpp
    io/buffers.hpp
    io/streams.hpp
    marshallers/marshallers.hpp
    messages/messages.hpp
    node/node.hpp
    options/options.hpp
    # Top-level headers - ensure these paths are correct relative to CMAKE_CURRENT_SOURCE_DIR
    config.hpp
    connectors.hpp
    formats.hpp
    interfaces.hpp
    io.hpp
    marshallers.hpp
    messages.hpp
    node.hpp
    manager/manager.hpp
    manager.hpp
    dns/dns.hpp
    dns/dns_poll.hpp
    dns.hpp
    metrics/metrics.hpp
    metrics.hpp
    options.hpp
    logging/logging.hpp
    logging.hpp
)

# Add the static library target
# CMake handles the platform-specific library name (e.g., libcbutils.a or cbutils.lib)
add_library(cbutils STATIC
    ${CBUTILS_SOURCES}
    # It's good practice to associate headers with the target for IDEs,
    # though they aren't "compiled" in the same way .cpp files are for a static lib.
    # Use PRIVATE if these headers are only for implementing cbutils.
    # Use PUBLIC if these headers are part of cbutils's public interface and consumers need them.
    # Given the mix, some might be private, some public.
    # For simplicity here, let's add them. A more granular approach might be needed.
    # ${CBUTILS_HEADERS} # This line is optional but good for IDEs.
)

# If the headers are used by the sources, their directories need to be in the include path.
# If all paths (config/config.cpp, config.hpp) are relative to this CMakeLists.txt location:
target_include_directories(cbutils
    PUBLIC # Or PRIVATE if these includes are only for compiling cbutils itself
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # If headers are also installed and used from an install interface:
        # $<INSTALL_INTERFACE:include> # Assuming headers are installed to 'include'
    PRIVATE
        # Add subdirectories if sources include headers relative to those subdirectories
        # e.g., if config.cpp includes "config.hpp" (same dir) and "foo/bar.hpp"
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/connector
        ${CMAKE_CURRENT_SOURCE_DIR}/formats
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/io
        ${CMAKE_CURRENT_SOURCE_DIR}/marshallers # This path was in headers but not sources, added for completeness
        ${CMAKE_CURRENT_SOURCE_DIR}/messages
        ${CMAKE_CURRENT_SOURCE_DIR}/node
        ${CMAKE_CURRENT_SOURCE_DIR}/manager
        ${CMAKE_CURRENT_SOURCE_DIR}/dns
        ${CMAKE_CURRENT_SOURCE_DIR}/metrics
        ${CMAKE_CURRENT_SOURCE_DIR} # For top-level headers like config.hpp
)
# A simpler approach if all includes are like `#include "config/config.hpp"`
# target_include_directories(cbutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Apply compile options (e.g., -O3 from AM_CXXFLAGS)
# These are PRIVATE because they are for building this library, not necessarily for its consumers.
# Consumers might link against a Release version of this lib which already has optimizations.
target_compile_options(cbutils PRIVATE -O3)

# For IDEs like Visual Studio to show header files in the project structure,
# or for some build system generators, explicitly listing headers with target_sources is better.
# If CBUTILS_HEADERS are indeed part of the library (either its interface or private implementation details)
target_sources(cbutils
    PRIVATE
        ${CBUTILS_HEADERS}
)
