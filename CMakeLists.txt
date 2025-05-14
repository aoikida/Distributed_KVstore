cmake_minimum_required(VERSION 3.10)
project(DistributedKVStore)

cmake_policy(SET CMP0144 NEW)
cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 17)

# Explicit Boost configuration
if(APPLE)
    set(BOOST_ROOT /opt/homebrew/opt/boost)
    set(BOOST_INCLUDEDIR /opt/homebrew/opt/boost/include)
    set(BOOST_LIBRARYDIR /opt/homebrew/opt/boost/lib)
endif()

find_package(Boost 1.76.0 REQUIRED COMPONENTS system thread date_time chrono)

# Library target with absolute paths
add_library(kv_store_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/anti_entropy/anti_entropy_manager.cpp
)

target_include_directories(kv_store_lib PUBLIC
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/anti_entropy
)

# Executables
add_executable(node1 ${CMAKE_CURRENT_SOURCE_DIR}/node1.cpp)
add_executable(node2 ${CMAKE_CURRENT_SOURCE_DIR}/node2.cpp)

target_link_libraries(node1 kv_store_lib ${Boost_LIBRARIES} pthread)
target_link_libraries(node2 kv_store_lib ${Boost_LIBRARIES} pthread)
