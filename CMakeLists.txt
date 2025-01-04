cmake_minimum_required(VERSION 3.15)
project(swimming_pool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(swimming_pool
        src/main.cpp
        src/pool/pool.cpp
        src/pool/pool.h
        src/lifeguard/lifeguard.cpp
        src/lifeguard/lifeguard.h
        src/cashier/cashier.cpp
        src/cashier/cashier.h
        src/client/client.cpp
        src/client/client.h
        src/pool_manager/pool_manager.cpp
        src/pool_manager/pool_manager.h
)

target_include_directories(swimming_pool PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/pool
        ${CMAKE_SOURCE_DIR}/src/pool_manager
        ${CMAKE_SOURCE_DIR}/src/lifeguard
        ${CMAKE_SOURCE_DIR}/src/cashier
        ${CMAKE_SOURCE_DIR}/src/client
)

find_package(Threads REQUIRED)

if(APPLE)
    target_link_libraries(swimming_pool PRIVATE
            Threads::Threads
    )
else()
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        target_link_libraries(swimming_pool PRIVATE
                Threads::Threads
                ${RT_LIBRARY}
        )
    else()
        target_link_libraries(swimming_pool PRIVATE
                Threads::Threads
        )
    endif()
endif()