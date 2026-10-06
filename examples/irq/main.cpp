#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rc522/card_reader.hpp"
#include "rc522/enums.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include "result_name.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <iostream>
#include <pico/platform/common.h>
#include <pico/time.h>
#include "hardware/sync.h"

static constexpr uint8_t irq_pin{6};

static volatile bool needs_to_poll{true};
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == irq_pin && (events & GPIO_IRQ_EDGE_FALL)) {
        needs_to_poll = true;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // SPI0: SCK=GP2, MOSI=GP3, MISO=GP4, CS=GP5
    auto result = spi_init(spi0, 11'000'000);
    std::cout << +result << std::endl;
    gpio_set_function(2, GPIO_FUNC_SPI);
    gpio_set_function(3, GPIO_FUNC_SPI);
    gpio_set_function(4, GPIO_FUNC_SPI);
    gpio_init(5);
    gpio_set_dir(5, GPIO_OUT);
    gpio_put(5, 1);
    gpio_init(irq_pin);
    gpio_set_dir(irq_pin, GPIO_IN);
    rc522::PicoTransport transport{5, spi0};
    rc522::Rc522 rc{transport};

    gpio_set_irq_enabled_with_callback(irq_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    if (rc.init(true) != rc522::result_of_op::SUCC) {
        printf("RC522 init failed, check wiring\n");
    } else {
        printf("RC522 init OK\n");
    }

    rc522::CardReader reader{rc};
    rc522::result_of_card res{};
    while (true) {
        reader.start_uid_transaction(rc522::WAKING_CARD_UP_FOR_UID::WUPA);
        needs_to_poll = true;
        auto time = get_absolute_time();
        while (true) {
            if (needs_to_poll) {
                needs_to_poll = false;
                res = reader.poll();
                if (res != rc522::result_of_card::WAIT)
                    break;
            } else {
                __wfi();
                // tight_loop_contents();
            }
        }
        if (res == rc522::result_of_card::SUCC) {
            auto time2 = get_absolute_time();
            auto uid = reader.uid();
            printf("UID: ");
            for (int i = 0; i < uid.size; i++)
                printf("%02X ", uid.bytes[i]);
            printf(" SAK: %02X\n", reader.Sak());
            printf("TIME: %lld\n", absolute_time_diff_us(time, time2));
        } else if (res == rc522::result_of_card::COLLISION) {
            printf("COLISSION APPEARED\n");
        }
    }
}
