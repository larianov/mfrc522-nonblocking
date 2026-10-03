cmake_minimum_required(VERSION 3.25)

include(pico_sdk_import.cmake)
project(rc522_examples LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
pico_sdk_init()

add_subdirectory(.. rc522)

foreach(name uid read write multi)
    add_executable(${name} ${name}/main.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    target_link_libraries(${name} PRIVATE rc522_pico)
    pico_enable_stdio_usb(${name} 1)
    pico_enable_stdio_uart(${name} 0)
    pico_add_extra_outputs(${name})

    add_custom_target(load_${name}
        COMMAND picotool load -f -x ${name}.uf2
        DEPENDS ${name}
        USES_TERMINAL
    )
endforeach()
