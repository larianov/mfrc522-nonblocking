#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <iostream>
#include <pico/platform/common.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include "rc522/card_reader.hpp"
#include "rc522/enums.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include "rc522/rc522.hpp"
#include "rc522/registers.hpp"


void get_uid(rc522::CardReader &rc){
    rc.start_uid_transaction();
    while (true) {
    auto res = rc.poll();
    if (res == rc522::result_of_card::WAIT) continue;
    if (res == rc522::result_of_card::SUCC) break;
    else return;
}
auto val = rc.uid();
printf("UID: ");
for (uint8_t i{}; i < val.size; i++) {
    printf("%02X ", val.bytes[i]);
}
printf("SAK: %02X", rc.Sak());
printf("\n");    
}


bool read(rc522::CardReader &rc){
    rc.start_read_transaction(5, std::array<uint8_t, 6>{0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF}, rc522::key::KeyA);
    while (true) {
        auto res = rc.poll();
        if (res == rc522::result_of_card::WAIT) continue;
        if (res == rc522::result_of_card::SUCC) break;
        else{
            return false;
        }
    }
    auto val = rc.block();
    printf("MAN BLOCK: ");
    for (uint8_t i{}; i < val.size; i++) {
        printf("%02X ", val.bytes[i]);
    }
    printf("SAK: %02X", rc.Sak());
    printf("\n");    
    return true;
}



bool increment(rc522::CardReader &rc){
    rc.start_alteration_op(5, std::array<uint8_t, 6>{0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF}, rc522::key::KeyA, 5, rc522::ALTERATION_OP::INCREMENT, 1);
    while (true) {
        auto res = rc.poll();
        if (res == rc522::result_of_card::WAIT) continue;
        if (res == rc522::result_of_card::SUCC) break;
        else{
            return false;
        }
    }
    return true;
}



bool write(rc522::CardReader &rc){
    std::array<uint8_t, 16>arr = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x05, 0xFA, 0x05, 0xFA};
    auto res = rc.start_write_transaction(5, std::array<uint8_t, 6>{0xFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF}, rc522::key::KeyA, arr, false);
    
    while (true) {
        auto val = rc.poll();
        if (val == rc522::result_of_card::SUCC) return true;
        else if (val == rc522::result_of_card::WAIT) continue;
        else {
            return false;
        }
    }
}



int main() {
    stdio_init_all();
    sleep_ms(2000);

    uint32_t spi_mask = (1u << 2) | (1u << 3) | (1u << 4);
    gpio_init(5);
    gpio_set_dir(5, GPIO_OUT);
    gpio_put(5, 1);
    gpio_set_function_masked(spi_mask, GPIO_FUNC_SPI);
    sleep_ms(200);
    spi_init(spi0, 1'000'000);

    rc522::PicoTransport pc{5, spi0};
    rc522::Rc522 rc{pc};
    if (rc.init() == rc522::result_of_op::SUCC) {
        std::cout << "PIS" << std::endl;
    }
    rc522::CardReader cr{rc};
    for (;;) {if(read(cr))break;}
    sleep_ms(500);
    for(;;){if(increment(cr))break;}
    for (;;) {if(read(cr))break;}
    for(;;) tight_loop_contents();
}
