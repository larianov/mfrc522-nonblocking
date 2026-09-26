#include "rc522/rc522.hpp"
#include "rc522/commands.hpp"
#include "rc522/registers.hpp"
#include "rc522/transport.hpp"
#include <cassert>
#include <cstdint>
#include <sys/_types.h>

Rc522::Rc522(Transport &t) : t_(t){};


void Rc522::write_one_byte(uint8_t address, uint8_t value_to_write){
    assert(address < 0x3C);
    uint8_t write_placeholder[2] = {static_cast<uint8_t>((address << 1U)), value_to_write};
    t_.transfer(write_placeholder, garbage, 2);
}

uint8_t Rc522::read_one_byte(uint8_t address){
    assert(address < 0x3C);
    uint8_t read_placeholder[2] = {0xFF, 0xFF};
    uint8_t write_placeholder[2] = {static_cast<uint8_t>((address << 1U) | (1U << 7U)), 0x00};
    t_.transfer(write_placeholder, read_placeholder, 2);
    return read_placeholder[1];
}

result_of_init Rc522::init(){
    write_one_byte(CommandReg, CMD_SOFT_RESET);
    uint8_t attempts{};
    while (attempts < 6) {
        auto result = read_one_byte(CommandReg);
        result = (result & POWER_DOWN);
        if (result == 0) {
            break;
        }
        attempts++;
        t_.delayUs(50);
    }
    if (attempts >= 6) return result_of_init::DEVICE_TIMEOUT;
    auto result_of_reading = read_one_byte(VersionReg);
    if (result_of_reading != 0x92 && result_of_reading != 0x91) {
        return result_of_init::WRONG_DEVICE;
    }
    write_one_byte(TModeReg, SET_UP_TIMER);
    write_one_byte(TPrescalerReg, SET_UP_TPRESCALER_LO);
    if (read_one_byte(TModeReg) != SET_UP_TIMER) return result_of_init::REGISTERES_NOT_CHANGING;
    if (read_one_byte(TPrescalerReg) != SET_UP_TPRESCALER_LO) return result_of_init::REGISTERES_NOT_CHANGING; 
    
    return result_of_init::SUCC;
}

// crc16 - 0x6363
// turn on antenna 
// tx1 - 1 1 1 X 
// tx2 - 1 1 0 0 X