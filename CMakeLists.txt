# Minimum CMake version required
cmake_minimum_required(VERSION 3.25)

# Define the project name, version (optional), and language
project(Cloudbus VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O3 -flto")

find_path(CARES_INCLUDE_DIR_MANUAL NAMES ares.h
    HINTS
        ENV Cares_ROOT      # User can set Cares_ROOT environment variable
        ENV CARES_ROOT      # User can set CARES_ROOT environment variable
        /usr/local/include
        /usr/include
    PATH_SUFFIXES cares # e.g. if headers are in include/cares/
)
find_library(CARES_LIBRARY_MANUAL NAMES cares libcares # Common names
    HINTS
        ENV Cares_ROOT
        ENV CARES_ROOT
        /usr/local/lib
        /usr/lib
)
if(CARES_LIBRARY_MANUAL)
    add_library(Cares::Manual UNKNOWN IMPORTED)
    set_target_properties(Cares::Manual PROPERTIES
        IMPORTED_LOCATION "${CARES_LIBRARY_MANUAL}"
    )
    target_include_directories(Cares::Manual INTERFACE "${CARES_INCLUDE_DIR_MANUAL}")
else()
    message(WARNING "Manual c-ares paths (CARES_LIBRARY_MANUAL) not found.")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED IMPORTED_TARGET libpcre2-8)
add_compile_definitions(PCRE2_CODE_UNIT_WIDTH=8)

# --- 1. Standard Installation Directories ---
# Include GNUInstallDirs to use standard variables like CMAKE_INSTALL_BINDIR,
# CMAKE_INSTALL_SYSCONFDIR, etc. This makes your installation paths standard.
include(GNUInstallDirs)
# --- 2. Custom Configuration Paths (for installation) ---
# Define the name of the subdirectory under the system configuration directory.
set(CLOUDBUS_CONF_SUBDIR "cloudbus")
# Construct the full installation path for the cloudbus configuration files.
set(CLOUDBUS_CONF_INSTALL_DIR "${CMAKE_INSTALL_SYSCONFDIR}/${CLOUDBUS_CONF_SUBDIR}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "RELEASE")
    add_compile_definitions("CONFDIR=\"${CLOUDBUS_CONF_INSTALL_DIR}\"")
    message(STATUS "CONFDIR will be defined by CMake as: ${CLOUDBUS_CONF_INSTALL_DIR}")
else()
    message(STATUS "CONFDIR will NOT be defined by CMake. User is expected to manage its definition/undefinition externally if needed.")
endif()

# Add the 'src' subdirectory. This directory must contain a CMakeLists.txt
# file that defines the 'cbutils' static library target.
add_subdirectory(src)
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
    message(STATUS "Test building enabled")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Test building disabled")
endif()

# Controller executable
set(CONTROLLER_CPPSOURCES
    src/cloudbus/controller/controller_connector.cpp
    src/cloudbus/controller/controller_marshaller.cpp
    src/main.cpp
)
add_executable(controller ${CONTROLLER_CPPSOURCES})
target_compile_definitions(controller PRIVATE COMPILE_CONTROLLER)
target_link_libraries(controller PRIVATE
    cbutils
    Cares::Manual
    PkgConfig::PCRE2
)

# Segment executable
set(SEGMENT_CPPSOURCES
    src/cloudbus/segment/segment_connector.cpp
    src/cloudbus/segment/segment_marshaller.cpp
    src/main.cpp
)
add_executable(segment ${SEGMENT_CPPSOURCES})
target_compile_definitions(segment PRIVATE COMPILE_SEGMENT)
target_link_libraries(segment PRIVATE
    cbutils
    Cares::Manual
    PkgConfig::PCRE2
)

# Install targets.
install(TARGETS controller segment RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Define the source files (paths relative to CMAKE_CURRENT_SOURCE_DIR or absolute)
# Assuming they are in a 'conf' subdirectory of where this CMakeLists.txt is.
set(CONTROLLER_ORIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/conf/controller.ini.orig")
set(SEGMENT_ORIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/conf/segment.ini.orig")
# Check if the source files exist (good practice)
if(NOT EXISTS "${CONTROLLER_ORIG_FILE}")
    message(FATAL_ERROR "Source file does not exist: ${CONTROLLER_ORIG_FILE}")
endif()
if(NOT EXISTS "${SEGMENT_ORIG_FILE}")
    message(FATAL_ERROR "Source file does not exist: ${SEGMENT_ORIG_FILE}")
endif()

# Install the files and rename them in the destination
install(FILES
    ${CONTROLLER_ORIG_FILE}  # Source file 1
    DESTINATION ${CLOUDBUS_CONF_INSTALL_DIR}
    RENAME
    "controller.ini"           # New name for source file 1
    COMPONENT configuration
)
install(FILES
    ${SEGMENT_ORIG_FILE}     # Source file 2
    DESTINATION ${CLOUDBUS_CONF_INSTALL_DIR}
    RENAME
    "segment.ini"              # New name for source file 2
    COMPONENT configuration
)
message(STATUS "controller.ini.orig will be installed as controller.ini in ${CMAKE_INSTALL_PREFIX}/${CLOUDBUS_CONF_INSTALL_DIR}")
message(STATUS "segment.ini.orig will be installed as segment.ini in ${CMAKE_INSTALL_PREFIX}/${CLOUDBUS_CONF_INSTALL_DIR}")
