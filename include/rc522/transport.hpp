#pragma once
#include "cstdint"

class Transport {
public:
    virtual void transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) = 0;
    virtual void delayUs(uint32_t us) = 0;
    virtual uint32_t microus_32() = 0;
    virtual ~Transport() = default;
};