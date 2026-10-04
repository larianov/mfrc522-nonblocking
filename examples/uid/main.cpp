#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rc522/card_reader.hpp"
#include "rc522/enums.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include "result_name.hpp"
#include <cstdio>
#include <pico/time.h>

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
    while (true) {
        reader.start_uid_transaction(rc522::WAKING_CARD_UP_FOR_UID::WUPA);
        rc522::result_of_card res{rc522::result_of_card::WAIT};
        while (res == rc522::result_of_card::WAIT) {
            res = reader.poll();
        }

        if (res == rc522::result_of_card::SUCC) {
            auto uid = reader.uid();
            printf("UID: ");
            for (int i = 0; i < uid.size; i++) printf("%02X ", uid.bytes[i]);
            printf(" SAK: %02X\n", reader.Sak());
        } else if (res == rc522::result_of_card::COLLISION) {
            printf("error: %s\n", result_name(res));
        }
    }
}
