#include "pico/stdlib.h"
#include "hardware/spi.h"
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
        else return;;
    }
    auto val = rc.uid();
    printf("UID: ");
    for (uint8_t i{}; i < val.size; i++) {
        printf("%02X ", val.bytes[i]);
    }
    printf("SAK: %02X", rc.Sak());
    printf("\n");
    
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
    rc.change_gain(rc522::RFCfgReg_Gain::DB_48);
    rc522::CardReader cr{rc};
    for (;;) {get_uid(cr);}
    
}
