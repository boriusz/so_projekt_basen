cmake_minimum_required(VERSION 3.15)
project(swimming_pool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(swimming_pool
        src/cashier/cashier.cpp
        src/cashier/cashier.h
        src/pool/pool.cpp
        src/pool/pool.h
        src/lifeguard/lifeguard.cpp
        src/lifeguard/lifeguard.h
        src/client/client.cpp
        src/client/client.h
        src/common/shared_memory.h
)

target_include_directories(swimming_pool PRIVATE
        src/cashier
        src/pool
        src/lifeguard
        src/client
        src/common
)

target_link_libraries(swimming_pool PRIVATE pthread)