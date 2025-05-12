# Define common header for tests if it exists and is needed for compilation listing
# Assuming tests.hpp is in a 'tests' subdirectory relative to this CMakeLists.txt
set(TEST_COMMON_HEADER tests.hpp) # Adjust path if different

# Define test executables
# Test for config
set(TEST_CONFIG_SOURCES test-config.cpp ${TEST_COMMON_HEADER})
add_executable(test-config ${TEST_CONFIG_SOURCES})
target_link_libraries(test-config PRIVATE cbutils) # LDADD = $(SOURCE)/libcbutils.a
add_test(NAME TestConfig COMMAND test-config)      # Register with CTest

# Test for interface
set(TEST_INTERFACE_SOURCES test-interface.cpp ${TEST_COMMON_HEADER})
add_executable(test-interface ${TEST_INTERFACE_SOURCES})
target_link_libraries(test-interface PRIVATE cbutils)
add_test(NAME TestInterface COMMAND test-interface)

# Test for metrics
set(TEST_METRICS_SOURCES test-metrics.cpp ${TEST_COMMON_HEADER})
add_executable(test-metrics ${TEST_METRICS_SOURCES})
target_link_libraries(test-metrics PRIVATE cbutils)
add_test(NAME TestMetrics COMMAND test-metrics)

# Test for messages
set(TEST_MESSAGES_SOURCES test-messages.cpp ${TEST_COMMON_HEADER})
add_executable(test-messages ${TEST_MESSAGES_SOURCES})
target_link_libraries(test-messages PRIVATE cbutils)
add_test(NAME TestMessages COMMAND test-messages)