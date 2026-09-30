#pragma once
#include "../../transport.hpp"
#include <cstdint>
#include <hardware/timer.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
namespace rc522{
class PicoTransport : public Transport {
  private:
    uint8_t cs_;
    spi_inst_t *spi_;

  public:
    PicoTransport(uint8_t cs, spi_inst_t *spi) : cs_(cs), spi_(spi) {};
    void transfer(const uint8_t *tx, uint8_t *rx, std::size_t len) override {
        gpio_put(cs_, false);
        spi_write_read_blocking(spi_, tx, rx, len);
        gpio_put(cs_, true);
    }
    void delayUs(uint32_t us) override {
        sleep_us(us);
    }
    uint32_t microus_32() override {
        return time_us_32();
    }
};
}