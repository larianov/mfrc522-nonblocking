cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)
endif()

project(rc522 VERSION 0.1.0 LANGUAGES C CXX ASM)

option(RC522_BUILD_EXAMPLES "Build rc522 examples" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
    if(PICO_SDK_VERSION_STRING VERSION_LESS "2.2.0")
        message(FATAL_ERROR
            "Pico SDK 2.2.0 or later required. Current: ${PICO_SDK_VERSION_STRING}")
    endif()

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    pico_sdk_init()
endif()

add_library(rc522 INTERFACE)
add_library(rc522::rc522 ALIAS rc522)

target_sources(rc522 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rc522.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/card_reader.cpp
)

target_include_directories(rc522 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(rc522 INTERFACE cxx_std_20)

if(PICO_SDK_VERSION_STRING)
    add_library(rc522_pico INTERFACE)
    add_library(rc522::pico ALIAS rc522_pico)

    target_sources(rc522_pico INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pico/spi_transport.cpp
    )

    target_link_libraries(rc522_pico INTERFACE
        rc522::rc522
        pico_stdlib
        hardware_spi
    )
endif()

if(RC522_BUILD_EXAMPLES)
    add_subdirectory(examples/basic)
endif()