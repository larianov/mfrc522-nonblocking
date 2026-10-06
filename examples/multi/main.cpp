#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rc522/card_reader.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include "result_name.hpp"
#include <array>
#include <cstdint>
#include <cstdio>

const uint8_t block = 5;
const std::array<uint8_t, 6> key{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool read_block(rc522::CardReader &reader) {
    reader.start_read_transaction(block, key, rc522::key::KeyA);
    rc522::result_of_card res{rc522::result_of_card::WAIT};
    while (res == rc522::result_of_card::WAIT) {
        res = reader.poll();
    }
    if (res != rc522::result_of_card::SUCC) {
        printf("read error: %s\n", result_name(res));
        return false;
    }
    auto data = reader.block();
    printf("block %d: ", block);
    for (int i = 0; i < data.size; i++)
        printf("%02X ", data.bytes[i]);
    printf("\n");
    return true;
}

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

    // value block format: value, ~value, value, addr, ~addr, addr, ~addr
    const std::array<uint8_t, 16> zero{0x00, 0x00, 0x00, 0x00, 0xFF,  0xFF, 0xFF,  0xFF,
                                       0x00, 0x00, 0x00, 0x00, block, 0xFA, block, 0xFA};

    while (true) {
        reader.start_write_transaction(block, key, rc522::key::KeyA, zero, false);
        rc522::result_of_card res{rc522::result_of_card::WAIT};
        while (res == rc522::result_of_card::WAIT) {
            res = reader.poll();
        }
        if (res == rc522::result_of_card::SUCC)
            break;
        if (res != rc522::result_of_card::TIMEOUT) {
            printf("write error: %s\n", result_name(res));
        }
        sleep_ms(500);
    }
    printf("block %d set to 0\n", block);
    read_block(reader);

    while (true) {
        sleep_ms(500);

        reader.start_alteration_op(block, key, rc522::key::KeyA, block, rc522::ALTERATION_OP::INCREMENT, 1);
        rc522::result_of_card res{rc522::result_of_card::WAIT};
        while (res == rc522::result_of_card::WAIT) {
            res = reader.poll();
        }
        if (res != rc522::result_of_card::SUCC) {
            printf("increment error: %s\n", result_name(res));
            continue;
        }
        read_block(reader);
    }
}
