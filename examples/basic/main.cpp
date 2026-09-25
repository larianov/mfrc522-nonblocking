#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <cstdint>
#include <hardware/gpio.h>
#include <iostream>
#include <pico/stdio.h>
#include <pico/time.h>

static constexpr uint8_t cs_io = 5;
static constexpr uint8_t VersionReg = (0x37 << 1) | (1u << 7); // 0x91 or 0x92
static constexpr uint8_t CommandReg_W = (0x01 << 1);
static constexpr uint8_t CommandReg_R = (0x01 << 1) | (1u << 7);


int main(){
    stdio_init_all();
    sleep_ms(2000);
    
    spi_init(spi0, 5'000'000);
    
    uint32_t spi_mask = (1u << 2) | (1u << 3) | (1u << 4);
    gpio_init(cs_io);
    gpio_set_dir(cs_io, GPIO_OUT);
    gpio_put(cs_io, 1);
    sleep_us(1);
    gpio_set_function_masked(spi_mask, GPIO_FUNC_SPI);
    
    gpio_put(cs_io, 0);
    uint8_t soft_reset[2] = {CommandReg_W, 0b0000'1111};
    std::cout << spi_write_blocking(spi0, soft_reset, 2) << std::endl;
    gpio_put(cs_io, 1);

    uint8_t result = 0b1111'1111;
    std::cout << get_absolute_time() << std::endl;
    while ((result & (1u << 4))) {
        gpio_put(cs_io, 0);
        spi_write_blocking(spi0, &CommandReg_R, 1);
        asm volatile("nop\n");
        spi_read_blocking(spi0, 0xFF, &result, 1);
        gpio_put(cs_io, 1);
    }
    std::cout << get_absolute_time() << std::endl;
    
    sleep_ms(2000);
    return 1;
}