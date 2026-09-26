#pragma once
#include "transport.hpp"
#include <cstdint>
enum class result_of_init : uint8_t{
    DEVICE_IS_NOT_RESPONDING,
    WRONG_DEVICE,
    WRONG_SOFT,
    DEVICE_TIMEOUT,
    REGISTERES_NOT_CHANGING,
    SUCC
};

class Rc522 {
private:
    uint8_t garbage[64]{};
    Transport& t_;
    void write_one_byte(uint8_t address, uint8_t value_to_write);
    uint8_t read_one_byte(uint8_t address);
public:
    explicit Rc522(Transport& t);
    result_of_init init();
};