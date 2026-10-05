#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rc522/card_reader.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include "result_name.hpp"
#include <array>
#include <cstdio>

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // SPI0: SCK=GP2, MOSI=GP3, MISO=GP4, CS=GP5
    spi_init(spi0, 1'000'000);
    gpio_set_function(2, GPIO_FUNC_SPI);
    gpio_set_function(3, GPIO_FUNC_SPI);
    gpio_set_function(4, GPIO_FUNC_SPI);
    gpio_init(5);
    gpio_set_dir(5, GPIO_OUT);
    gpio_put(5, 1);

    rc522::PicoTransport transport{5, spi0};
    rc522::Rc522 rc{transport};
    if (rc.init(false) != rc522::result_of_op::SUCC) {
        printf("RC522 init failed, check wiring\n");
    } else {
        printf("RC522 init OK\n");
    }
    rc522::CardReader reader{rc};

    const uint8_t block = 4;
    const std::array<uint8_t, 6> key{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::array<uint8_t, 16> data{'h', 'e', 'l', 'l', 'o', ' ', 'D', 'a', 'v', 'i', 'd'};

    while (true) {
        reader.start_write_transaction(block, key, rc522::key::KeyA, data, false);
        rc522::result_of_card res{rc522::result_of_card::WAIT};
        while (res == rc522::result_of_card::WAIT) {
            res = reader.poll();
        }

        if (res == rc522::result_of_card::SUCC) {
            printf("block %d written\n", block);
            break;
        }
        if (res != rc522::result_of_card::TIMEOUT) {
            printf("error: %s\n", result_name(res));
        }
        sleep_ms(500);
    }

    while (true) tight_loop_contents();
}
