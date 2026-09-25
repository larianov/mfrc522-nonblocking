add_executable(rc522_basic main.cpp)

set_source_files_properties(main.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wshadow;-Wnull-dereference;-Wpedantic"
)

target_compile_options(rc522_basic PRIVATE
    $<$<CONFIG:Debug>:-Og;-ggdb3>
)

pico_enable_stdio_uart(rc522_basic 0)
pico_enable_stdio_usb(rc522_basic 1)

target_link_libraries(rc522_basic PRIVATE
    rc522::pico
)

pico_add_extra_outputs(rc522_basic)

find_program(PICOTOOL_EXECUTABLE picotool)

if(PICOTOOL_EXECUTABLE)
    add_custom_target(load
        COMMAND ${PICOTOOL_EXECUTABLE} load -f -x
                $<TARGET_FILE_DIR:rc522_basic>/rc522_basic.uf2
        DEPENDS rc522_basic
        COMMENT "Loading rc522_basic.uf2 to Pico"
        USES_TERMINAL
        VERBATIM
    )
else()
    message(WARNING "picotool was not found: target 'load' will not be available")
endif()