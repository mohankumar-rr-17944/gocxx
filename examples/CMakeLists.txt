# Examples CMakeLists.txt for gocxx

cmake_minimum_required(VERSION 3.15)

# This file can be used standalone or included from the main CMakeLists.txt

# If building standalone, we need to find or build gocxx
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(gocxx_examples CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    
    # Try to find gocxx package
    find_package(gocxx QUIET)
    
    if(NOT gocxx_FOUND)
        # Build gocxx from parent directory
        message(STATUS "gocxx not found, building from parent directory")
        add_subdirectory(.. gocxx_build)
    endif()
endif()

# Function to add an example executable
function(add_gocxx_example example_name source_file)
    add_executable(${example_name} ${source_file})
    target_link_libraries(${example_name} PRIVATE gocxx)
    
    # Set output directory
    set_target_properties(${example_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    message(STATUS "Added example: ${example_name}")
endfunction()

# Add all examples
# add_gocxx_example(context_example context_example.cpp)  # TODO: Fix context API usage
add_gocxx_example(tcp_example tcp_example.cpp)
add_gocxx_example(udp_example udp_example.cpp)
add_gocxx_example(http_example http_example.cpp)

# Optional: Create a target to build all examples
add_custom_target(examples
    DEPENDS 
        # context_example
        tcp_example
        udp_example
        http_example
    COMMENT "Building all gocxx examples"
)

message(STATUS "Configured ${CMAKE_CURRENT_LIST_DIR} examples")
