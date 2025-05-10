cmake_minimum_required(VERSION 3.14)
project(DistributedKVStore)

cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 17)

# Explicitly find Homebrew's Boost installation
if(APPLE)
    # macOS-specific Boost path with homebrew
    set(BOOST_ROOT /opt/homebrew)
    set(BOOST_INCLUDEDIR /opt/homebrew/include)
    set(BOOST_LIBRARYDIR /opt/homebrew/lib)
endif()

# Find Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_DEBUG ON)
find_package(Boost 1.88.0 REQUIRED COMPONENTS system thread)

# Include directories
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/anti_entropy)

# Create a library with common sources
add_library(kv_store_lib STATIC 
    node.cpp 
    anti_entropy/anti_entropy_manager.cpp
)

# Add executables
add_executable(node1 node1.cpp)
add_executable(node2 node2.cpp)

# Link common library to both executables
target_link_libraries(node1 kv_store_lib ${Boost_LIBRARIES} pthread)
target_link_libraries(node2 kv_store_lib ${Boost_LIBRARIES} pthread)
