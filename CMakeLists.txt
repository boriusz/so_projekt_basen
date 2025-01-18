cmake_minimum_required(VERSION 3.15)
project(swimming_pool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
        src/pool_manager/pool_manager.cpp
        src/working_hours_manager/working_hours_manager.h
        src/error_handler/error_handler.cpp
        src/ui_manager/ui_manager.cpp
        src/pool/pool.cpp
)

set(MAIN_SOURCES
        src/lifeguard/lifeguard.cpp
        src/cashier/cashier.cpp
        src/client/client.cpp
        src/maintenance_manager/maintenance_manager.cpp
        src/common/signal_handler.cpp
        src/ticket/ticket.cpp
)

add_executable(swimming_pool
        src/main.cpp
        ${MAIN_SOURCES}
        ${COMMON_SOURCES}
)

add_executable(monitor
        src/monitor/monitor.cpp
        ${MAIN_SOURCES}
        ${COMMON_SOURCES}
)

set(COMMON_INCLUDES
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/pool
        ${CMAKE_SOURCE_DIR}/src/pool_manager
        ${CMAKE_SOURCE_DIR}/src/working_hours_manager
        ${CMAKE_SOURCE_DIR}/src/error_handler
        ${CMAKE_SOURCE_DIR}/src/ui_manager
)

target_include_directories(swimming_pool PRIVATE
        ${COMMON_INCLUDES}
        ${CMAKE_SOURCE_DIR}/src/lifeguard
        ${CMAKE_SOURCE_DIR}/src/cashier
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/src/maintenance_manager
        ${CMAKE_SOURCE_DIR}/src/ticket
)

target_include_directories(monitor PRIVATE
        ${COMMON_INCLUDES}
        ${CMAKE_SOURCE_DIR}/src/monitor
        ${CMAKE_SOURCE_DIR}/src/ticket
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/src/cashier
)

target_compile_definitions(monitor PRIVATE MONITOR_BUILD)

find_package(Threads REQUIRED)

function(configure_target TARGET)
    if(APPLE)
        target_link_libraries(${TARGET} PRIVATE
                Threads::Threads
        )
    else()
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(${TARGET} PRIVATE
                    Threads::Threads
                    ${RT_LIBRARY}
            )
        else()
            target_link_libraries(${TARGET} PRIVATE
                    Threads::Threads
            )
        endif()
    endif()
endfunction()

configure_target(swimming_pool)
configure_target(monitor)