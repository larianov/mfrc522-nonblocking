cmake_minimum_required(VERSION 3.25)
project(rc522 LANGUAGES CXX)

add_library(rc522 STATIC src/rc522.cpp src/card_reader.cpp)
target_include_directories(rc522 PUBLIC include)
target_compile_features(rc522 PUBLIC cxx_std_17)

if(PICO_SDK_VERSION_STRING)
    add_library(rc522_pico INTERFACE)
    target_link_libraries(rc522_pico INTERFACE rc522 pico_stdlib hardware_spi)
endif()
