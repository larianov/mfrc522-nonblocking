#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rc522/card_reader.hpp"
#include "rc522/platform/pico/spi_transport.hpp"
#include <array>
#include <cstdio>
#include <cstring>

using rc522::result_of_card;

const std::array<uint8_t, 6> key{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const std::array<uint8_t, 6> wrong_key{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

int passed = 0;
int failed = 0;

void check(const char *name, bool ok) {
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) passed++;
    else failed++;
}

bool read_value(rc522::CardReader &reader, uint8_t block, int32_t &value) {
    reader.start_read_transaction(block, key, rc522::key::KeyA);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();
    if (res != result_of_card::SUCC) return false;
    auto data = reader.block();
    value = data.bytes[0] | data.bytes[1] << 8 | data.bytes[2] << 16 | data.bytes[3] << 24;
    return true;
}

void test_uid_stable(rc522::CardReader &reader) {
    reader.start_read_transaction(4, key, rc522::key::KeyA);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();
    auto first = reader.uid();

    int same = 0;
    uint32_t start = time_us_32();
    for (int i = 0; i < 100; i++) {
        reader.start_read_transaction(4, key, rc522::key::KeyA);
        res = result_of_card::WAIT;
        while (res == result_of_card::WAIT) res = reader.poll();
        auto uid = reader.uid();
        if (res == result_of_card::SUCC && uid.size == first.size && std::memcmp(uid.bytes, first.bytes, uid.size) == 0) {
            same++;
        }
    }
    printf("  %d/100 same UID, read takes %lu us\n", same, (time_us_32() - start) / 100);
    check("UID stable over 100 reads", same == 100);
}

void test_write_read(rc522::CardReader &reader, uint8_t fill) {
    std::array<uint8_t, 16> data{};
    data.fill(fill);

    reader.start_write_transaction(4, key, rc522::key::KeyA, data, false);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();
    bool written = res == result_of_card::SUCC;

    reader.start_read_transaction(4, key, rc522::key::KeyA);
    res = result_of_card::WAIT;
    while (res == result_of_card::WAIT) res = reader.poll();
    bool same = res == result_of_card::SUCC && std::memcmp(reader.block().bytes, data.data(), 16) == 0;

    char name[48];
    snprintf(name, sizeof(name), "write 0x%02X to block 4, read back", fill);
    check(name, written && same);
}

void test_wrong_key(rc522::CardReader &reader) {
    reader.start_read_transaction(4, wrong_key, rc522::key::KeyA);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();
    check("wrong key -> AUTH_FAILED", res == result_of_card::AUTH_FAILED);

    reader.start_read_transaction(4, key, rc522::key::KeyA);
    res = result_of_card::WAIT;
    while (res == result_of_card::WAIT) res = reader.poll();
    check("read works right after AUTH_FAILED", res == result_of_card::SUCC);
}

void test_guards(rc522::CardReader &reader) {
    std::array<uint8_t, 16> data{};
    check("write block 0 -> OP_NOT_POSSIBLE",
          reader.start_write_transaction(0, key, rc522::key::KeyA, data, true) == result_of_card::OP_NOT_POSSIBLE);
    check("write trailer without REQUIRED -> OP_NOT_POSSIBLE",
          reader.start_write_transaction(7, key, rc522::key::KeyA, data, false) == result_of_card::OP_NOT_POSSIBLE);
}

void test_value(rc522::CardReader &reader) {
    const std::array<uint8_t, 16> zero{0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                                       0x00, 0x00, 0x00, 0x00, 0x05, 0xFA, 0x05, 0xFA};
    reader.start_write_transaction(5, key, rc522::key::KeyA, zero, false);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();
    int32_t value = -1;
    check("value block 5 set to 0", res == result_of_card::SUCC && read_value(reader, 5, value) && value == 0);

    bool all_ok = true;
    uint32_t start = time_us_32();
    for (int i = 0; i < 10; i++) {
        reader.start_alteration_op(5, key, rc522::key::KeyA, 5, rc522::ALTERATION_OP::INCREMENT, 1);
        res = result_of_card::WAIT;
        while (res == result_of_card::WAIT) res = reader.poll();
        if (res != result_of_card::SUCC) all_ok = false;
    }
    printf("  increment takes %lu us\n", (time_us_32() - start) / 10);
    check("increment x10 -> 10", all_ok && read_value(reader, 5, value) && value == 10);

    reader.start_alteration_op(5, key, rc522::key::KeyA, 5, rc522::ALTERATION_OP::DECREMENT, 3);
    res = result_of_card::WAIT;
    while (res == result_of_card::WAIT) res = reader.poll();
    check("decrement 3 -> 7", res == result_of_card::SUCC && read_value(reader, 5, value) && value == 7);

    reader.start_alteration_op(5, key, rc522::key::KeyA, 6, rc522::ALTERATION_OP::RESTORE, 0);
    res = result_of_card::WAIT;
    while (res == result_of_card::WAIT) res = reader.poll();
    check("restore 5 -> 6, block 6 = 7", res == result_of_card::SUCC && read_value(reader, 6, value) && value == 7);
}

void test_busy(rc522::CardReader &reader) {
    auto res = reader.start_read_transaction(4, key, rc522::key::KeyA);
    check("second start while busy -> OP_NOT_POSSIBLE",
          res == result_of_card::SUCC &&
              reader.start_read_transaction(4, key, rc522::key::KeyA) == result_of_card::OP_NOT_POSSIBLE);

    reader.abort();
    for (int attempt = 0; attempt < 3; attempt++) {
        reader.start_read_transaction(4, key, rc522::key::KeyA);
        res = result_of_card::WAIT;
        while (res == result_of_card::WAIT) res = reader.poll();
        if (res == result_of_card::SUCC) break;
    }
    check("reader usable after abort()", res == result_of_card::SUCC);
}

void test_halt(rc522::CardReader &reader) {
    reader.start_read_transaction(4, key, rc522::key::KeyA);
    result_of_card res{result_of_card::WAIT};
    while (res == result_of_card::WAIT) res = reader.poll();

    reader.start_uid_transaction();
    res = result_of_card::WAIT;
    while (res == result_of_card::WAIT) res = reader.poll();
    check("halted card ignores REQA", res == result_of_card::TIMEOUT);
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
    uint8_t version{};
    bool init_ok = rc.init(false) == rc522::result_of_op::SUCC;
    printf("%s\n", rc.version(version));
    check("init", init_ok);
    if (!init_ok) {
        while (true) tight_loop_contents();
    }
    rc522::CardReader reader{rc};

    printf("Put a MIFARE Classic 1K test card on the reader (blocks 4, 5, 6 will be overwritten)\n");
    while (true) {
        reader.start_read_transaction(4, key, rc522::key::KeyA);
        result_of_card res{result_of_card::WAIT};
        while (res == result_of_card::WAIT) res = reader.poll();
        if (res == result_of_card::SUCC) break;
        sleep_ms(200);
    }
    printf("Card found, running tests\n\n");

    test_uid_stable(reader);
    test_write_read(reader, 0xAA);
    test_write_read(reader, 0x55);
    test_wrong_key(reader);
    test_guards(reader);
    test_value(reader);
    test_busy(reader);
    test_halt(reader);

    printf("\n%d passed, %d failed\n", passed, failed);
    while (true) tight_loop_contents();
}
