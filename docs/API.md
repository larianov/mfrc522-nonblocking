# 📚 API Reference

Everything lives in the `rc522` namespace. Three classes, from the bottom up:

```
CardReader  ──▶  Rc522  ──▶  Transport
```

- [Transport](#-transport) — the platform interface
- [Porting to your platform](#-porting-to-your-platform) — one class, example for STM32
- [PicoTransport](#-picotransport) — ready-made transport for the Raspberry Pi Pico
- [Rc522](#-rc522) — the chip
- [CardReader](#-cardreader) — card operations
- [Results](#-results) — what every result code means
- [Troubleshooting](#-troubleshooting) — what to do with each result, symptom → cause
- [Types](#-types) — `Uid`, `Read_Block`, enums

---

## 🔌 Transport

`#include "rc522/transport.hpp"`

The only thing the driver needs from a platform. Implement it to run on
something other than the Pico.

```cpp
class Transport {
  public:
    virtual void transfer(const uint8_t *tx, uint8_t *rx, std::size_t len) = 0;
    virtual void delayUs(uint32_t us) = 0;
    virtual uint32_t microus_32() = 0;
    virtual ~Transport() = default;
};
```

| Method       | Must do                                                                      |
| ------------ | ---------------------------------------------------------------------------- |
| `transfer`   | pull CS low, full-duplex SPI transfer of `len` bytes, pull CS high. `tx` and `rx` may be **the same buffer** |
| `delayUs`    | block for `us` microseconds (used only in `init`, `set_power_state`, `change_gain`) |
| `microus_32` | free-running microsecond counter                                             |

SPI mode 0, MSB first, up to 10 MHz.

---

## 🛠 Porting to Your Platform

The whole port is one class derived from `Transport`. Everything above it —
`Rc522`, `CardReader`, all state machines — is plain C++17 and doesn't change.

### 1. Write the transport

Example for **STM32 HAL** (blocking SPI, microseconds from the DWT cycle counter):

```cpp
#include "rc522/transport.hpp"
#include "stm32f4xx_hal.h"

class Stm32Transport : public rc522::Transport {
  private:
    SPI_HandleTypeDef *spi_;
    GPIO_TypeDef *cs_port_;
    uint16_t cs_pin_;

  public:
    Stm32Transport(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
        : spi_(spi), cs_port_(cs_port), cs_pin_(cs_pin) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   // enable the cycle counter
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    void transfer(const uint8_t *tx, uint8_t *rx, std::size_t len) override {
        HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_RESET);
        HAL_SPI_TransmitReceive(spi_, const_cast<uint8_t *>(tx), rx, len, HAL_MAX_DELAY);
        HAL_GPIO_WritePin(cs_port_, cs_pin_, GPIO_PIN_SET);
    }
    void delayUs(uint32_t us) override {
        uint32_t start = DWT->CYCCNT;
        uint32_t cycles = us * (SystemCoreClock / 1'000'000);
        while (DWT->CYCCNT - start < cycles) {}
    }
    uint32_t microus_32() override {
        return DWT->CYCCNT / (SystemCoreClock / 1'000'000);
    }
};
```

The ready-made [`PicoTransport`](../include/rc522/platform/pico/spi_transport.hpp)
is the same thing for the Pico SDK.

Things to get right:

- **CS around the whole transfer** — the chip reads the register address from the
  first byte after CS goes low, so CS must stay low for all `len` bytes
- **`tx` and `rx` can be the same buffer** — FIFO reads use one buffer for both.
  Byte-by-byte full-duplex SPI handles this fine; a DMA transfer may not
- **Configure SPI and the CS pin before** creating `Rc522` — CS idle high

### 2. Link the library

```cmake
add_subdirectory(rc522)
target_link_libraries(your_app PRIVATE rc522)
```

The `rc522` target needs only a C++17 compiler. (`rc522_pico` is the Pico
variant: `rc522` + `pico_stdlib` + `hardware_spi`.)

### 3. Use it

```cpp
Stm32Transport transport{&hspi1, GPIOA, GPIO_PIN_4};
rc522::Rc522 rc{transport};
rc.init(false);
rc522::CardReader reader{rc};
```

From here the code is the same as in the [examples](../examples).

---

## 🍓 PicoTransport

`#include "rc522/platform/pico/spi_transport.hpp"` — available when linking `rc522_pico`.

```cpp
PicoTransport(uint8_t cs, spi_inst_t *spi);
```

| Parameter | Meaning                                          |
| --------- | ------------------------------------------------ |
| `cs`      | GPIO used as chip select                         |
| `spi`     | `spi0` or `spi1`                                 |

The transport doesn't configure anything — before using it, call `spi_init`, set
the SPI pins to `GPIO_FUNC_SPI`, and init `cs` as an output set **high**:

```cpp
spi_init(spi0, 1'000'000);
gpio_set_function(2, GPIO_FUNC_SPI);   // SCK
gpio_set_function(3, GPIO_FUNC_SPI);   // MOSI
gpio_set_function(4, GPIO_FUNC_SPI);   // MISO
gpio_init(5);
gpio_set_dir(5, GPIO_OUT);
gpio_put(5, 1);

rc522::PicoTransport transport{5, spi0};
```

---

## 🔧 Rc522

`#include "rc522/rc522.hpp"`

The chip itself. Holds a reference to the transport — the transport must outlive it.

```cpp
explicit Rc522(Transport &t);
```

### `init`

```cpp
result_of_op init(bool set_up_irq);
```

Soft-resets the chip, checks its version and configures it: timer, CRC preset
for ISO 14443A, 100% ASK, antenna on. Call it once before anything else.

| `set_up_irq` | Effect                                                                       |
| ------------ | ---------------------------------------------------------------------------- |
| `false`      | `IRQ` pin is not used                                                        |
| `true`       | `IRQ` pin goes **low** when a transfer receives data, finishes, fails or times out — wait for a **falling edge** |

| Returns                   | When                                                           |
| ------------------------- | -------------------------------------------------------------- |
| `SUCC`                    | ready                                                          |
| `DEVICE_TIMEOUT`          | the chip didn't come out of the soft reset — check wiring / power |
| `WRONG_DEVICE`            | `VersionReg` isn't `0x91` or `0x92` |
| `REGISTERES_NOT_CHANGING` | a written register reads back different — check MISO / SPI speed |

> With no chip on the bus `init` gives `DEVICE_TIMEOUT` or `WRONG_DEVICE`,
> depending on whether MISO floats high or low.

### `version`

```cpp
const char *version(uint8_t &version_mut) const;
```

Writes the raw `VersionReg` value to `version_mut` and returns a readable string:
`"Version: 1.0"` (`0x91`), `"Version: 2.0"` (`0x92`), `"Unknown version"`. Before
`init` — `0xFF` and `"Version can't be determined untill init."`.

### `change_gain`

```cpp
result_of_op change_gain(RFCfgReg_Gain value);
```

Sets the receiver gain: `DB_18`, `DB_23`, `DB_33`, `DB_38`, `DB_43`, `DB_48`.
Higher gain — longer read distance, more sensitive to noise. Blocks for 6 ms.

| Returns                    | When                                   |
| -------------------------- | -------------------------------------- |
| `SUCC`                     | gain set                               |
| `DEVICE_IS_NOT_RESPONDING` | register read gave `0xFF`              |
| `REGISTERES_NOT_CHANGING`  | value didn't stick                     |

### `set_power_state`

```cpp
result_of_op set_power_state(uint8_t power_up);   // 1 = power up, 0 = soft power-down
```

Returns `SUCC`, or `DEVICE_TIMEOUT` if the chip didn't switch within ~400 µs.

### Low-level methods

These are public, but `CardReader` uses them for you — you only need them to
write your own card protocol.

| Method                     | Does                                                              |
| -------------------------- | ----------------------------------------------------------------- |
| `start_exc(arr, size, byt7e, tx_crc, timeout, way, rx_crc)` | loads `arr` into the FIFO and starts a Transceive (or MFAuthent) with the chosen timeout and CRC |
| `check_exc()`              | `WAIT` while the transfer runs, then `SUCC` / `Time_out` / an error |
| `recieve_exc(arr, size)`   | reads exactly `size` bytes from the FIFO, `ProtocolErr` if a different count arrived |
| `check_auth()`             | like `check_exc()`, but for MFAuthent                             |
| `clear_mauth()`            | turns Crypto1 off                                                 |
| `read_one_byte(addr)`      | reads one register                                                |
| `get_isr_flag()`           | `true` while a transfer is in flight                              |
| `clear_isr_flag()`         | resets that flag                                                  |
| `get_time()`               | `Transport::microus_32()`                                         |

---

## 💳 CardReader

`#include "rc522/card_reader.hpp"`

Card operations as non-blocking state machines. Holds a reference to `Rc522`.

```cpp
explicit CardReader(Rc522 &ic_ref);
```

### How an operation runs

```cpp
reader.start_read_transaction(4, key, rc522::key::KeyA);   // 1. start
rc522::result_of_card res{rc522::result_of_card::WAIT};
while (res == rc522::result_of_card::WAIT) {
    res = reader.poll();                                   // 2. poll
}
auto data = reader.block();                                // 3. result
```

1. **`start_*()`** only checks the arguments and remembers the operation. Nothing
   is sent yet. Returns `SUCC` or `OP_NOT_POSSIBLE`.
2. **`poll()`** advances the operation until it has to wait for the card and
   returns `WAIT`, or until it's done and returns the final result. After a
   final result the reader is free for the next `start_*()`.
3. **Getters** give the data of the last finished operation.

Every operation ends by putting the card into **HALT**.

### `start_uid_transaction`

```cpp
result_of_card start_uid_transaction(WAKING_CARD_UP_FOR_UID wc = WAKING_CARD_UP_FOR_UID::REQA);
```

Selects the card and reads its UID and SAK. Works with any ISO 14443A card.

| `wc`   | Wakes                       | Use when                                              |
| ------ | --------------------------- | ----------------------------------------------------- |
| `REQA` | only cards that aren't halted | you want each card **once** — a card left on the reader gives `TIMEOUT` until it's taken away and brought back |
| `WUPA` | halted cards too            | you want to read the card **again and again** while it lies on the reader |

Result: [`uid()`](#getters), [`Sak()`](#getters).

### `start_read_transaction`

```cpp
result_of_card start_read_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv);
```

Authenticates with `keybuff` as Key A or Key B and reads 16 bytes from `block`.

Result: [`block()`](#getters), and `uid()` / `Sak()` of the card.

### `start_write_transaction`

```cpp
result_of_card start_write_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv,
                                       std::array<uint8_t, 16> write_buff, bool REQUIRED);
```

Authenticates and writes `write_buff` into `block`.

`start` returns `OP_NOT_POSSIBLE` when:

- `block == 0` — the manufacturer block
- `block` is a **sector trailer** (3, 7, 11 … 127, then 143, 159 … 255) and
  `REQUIRED == false`
- `block` is a sector trailer, `REQUIRED == true`, but the access bits in bytes
  6–8 of `write_buff` are not consistent (each nibble must have its inverted copy).
  Inconsistent access bits would lock the sector forever

`REQUIRED` means "yes, I really want to write a trailer". For data blocks it's ignored.

### `start_alteration_op`

```cpp
result_of_card start_alteration_op(uint8_t block_src, std::array<uint8_t, 6> keybuff, key keyv,
                                   uint8_t block_dst, ALTERATION_OP oper, int32_t operand);
```

Value block operation on `block_src`, result transferred into `block_dst`
(can be the same block).

| `oper`      | Result in `block_dst`       | `operand`        |
| ----------- | --------------------------- | ---------------- |
| `INCREMENT` | `value(src) + operand`      | amount to add    |
| `DECREMENT` | `value(src) - operand`      | amount to subtract |
| `RESTORE`   | `value(src)` — a copy       | ignored          |

`start` returns `OP_NOT_POSSIBLE` when either block is block 0 or a sector
trailer, or when they're in different sectors.

`block_src` must already be in **value block format**, otherwise the card
answers with NAK and `poll()` returns `OP_NOT_POSSIBLE`:

```
bytes 0–3    value     (int32, little endian)
bytes 4–7    ~value
bytes 8–11   value
bytes 12–15  addr, ~addr, addr, ~addr
```

### Memory operations and card types

Read, write and alteration authenticate with MIFARE Classic Crypto1, so the card
is checked by its SAK. `poll()` returns `OP_NOT_POSSIBLE` if:

| SAK     | Card                | Allowed blocks |
| :-----: | ------------------- | :------------: |
| `09`    | MIFARE Classic Mini | 0–19           |
| `08`    | MIFARE Classic 1K   | 0–63           |
| `18`    | MIFARE Classic 4K   | 0–255          |
| other   | —                   | none           |

### `poll`

```cpp
result_of_card poll();
```

Advances the current operation. Returns `WAIT` while the card hasn't answered
yet, otherwise the final [result](#-results). Returns `OP_NOT_POSSIBLE` if no
operation is running (never started, refused by `start`, or already finished).

- **Without IRQ** — call it in a loop, do other work between calls
- **With IRQ** (`init(true)`) — call it once after `start`, then again after
  each falling edge on the `IRQ` pin

### `abort`

```cpp
void abort();
```

Drops the current operation, so the next `start_*()` is accepted. The card is not
halted — it stays selected until the next operation wakes it again.

### Getters

```cpp
Uid uid() const;           // UID of the last selected card
uint8_t Sak() const;       // its SAK
Read_Block block() const;  // data of the last successful read
```

`uid()` / `Sak()` are filled by every operation (all of them select the card).
The values stay until the next operation overwrites them.

---

## 📋 Results

### `result_of_card` — `CardReader`

| Result            | Meaning                                         | Typical cause                                   |
| ----------------- | ----------------------------------------------- | ----------------------------------------------- |
| `WAIT`            | operation is still running                      | call `poll()` again                             |
| `SUCC`            | `start`: accepted. `poll`: done                  |                                                 |
| `TIMEOUT`         | card didn't answer in time                      | no card, card too far, halted card + REQA       |
| `COLLISION`       | several cards answered at once                  | more than one card in the field                 |
| `BITERROR`        | data arrived broken                             | CRC / parity / BCC error, unexpected answer — card moved, noise |
| `AUTH_FAILED`     | authentication failed                           | wrong key, wrong key type (A/B), card left      |
| `OP_NOT_POSSIBLE` | operation refused                               | guards from `start_*`, reader busy, `poll()` without an operation, card isn't MIFARE Classic, block out of range, card NAK (access bits forbid it, not a value block) |
| `ERROR_FROM_IC`   | the chip reported an error                      | protocol error, overheating                     |
| `NO_CARD`         | not returned yet — no card gives `TIMEOUT`      |                                                 |
| `KEY_WAS_REJECTED`| not returned yet — a wrong key gives `AUTH_FAILED` |                                              |

### `result_of_op` — `Rc522`

| Result                     | Meaning                                    |
| -------------------------- | ------------------------------------------ |
| `SUCC`                     | done                                       |
| `DEVICE_TIMEOUT`           | chip didn't reach the expected state       |
| `WRONG_DEVICE`             | version isn't `0x91` / `0x92`              |
| `REGISTERES_NOT_CHANGING`  | register reads back a different value      |
| `DEVICE_IS_NOT_RESPONDING` | register read gave `0xFF`                  |
| `WRONG_SOFT`               | not returned yet                           |

---

## 🩺 Troubleshooting

### What to do with each result

| Result            | Retry?                 | What to do                                                                 |
| ----------------- | :--------------------: | -------------------------------------------------------------------------- |
| `TIMEOUT`         | ✅ yes                 | normal when no card is there — just start again later. If a card *is* there, see the table below |
| `BITERROR`        | ✅ yes                 | the card moved or the signal is noisy — start the same operation again     |
| `COLLISION`       | ✅ after removing a card | only one card at a time is supported                                     |
| `AUTH_FAILED`     | ⚠️ once                | one retry covers a card that moved during authentication. If it fails again, the key or key type is wrong — retrying won't help |
| `OP_NOT_POSSIBLE` | ❌ no                  | the request itself is wrong — see below                                    |
| `ERROR_FROM_IC`   | ✅ yes                 | if it repeats, call `Rc522::init()` again                                  |

> After an **error** the card is *not* halted — HALT is sent only after a
> successful operation. So a retry finds the card even with REQA.

`OP_NOT_POSSIBLE` from **`start_*()`**:

- another operation is still running — finish it with `poll()` or call `abort()`
- block 0, or a sector trailer without `REQUIRED`, or a trailer with inconsistent access bits
- value operation: `src` / `dst` in different sectors, or one of them is block 0 / a trailer

`OP_NOT_POSSIBLE` from **`poll()`**:

- no operation is running (`start_*()` refused it or it already finished)
- the card isn't MIFARE Classic (SAK not `08` / `18` / `09`), or the block is beyond its size
- the card answered NAK — the access bits of the sector don't allow this operation
  with this key, or the block isn't in value block format (for increment / decrement / restore)

### Symptoms

| Symptom                                           | Likely cause                                                                           |
| ------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `init` → `DEVICE_TIMEOUT`                         | no power, `RST` not tied high, MISO not connected                                       |
| `init` → `WRONG_DEVICE`                           | print `version()`: `0x00` / `0xFF` → no SPI communication (wiring, CS pin, SPI mode); any other value → a clone chip, only `0x91` / `0x92` are accepted |
| `init` → `REGISTERES_NOT_CHANGING`                | writes don't stick — SPI too fast, long wires, bad MISO contact                         |
| Always `TIMEOUT`, card is on the reader           | card was halted and `start_uid_transaction()` uses REQA → use `WUPA`; card too far (RC522 reads ~1–3 cm); metal behind the card; try `change_gain(RFCfgReg_Gain::DB_48)` |
| UID is read only once per card                    | that's REQA — see above. Use `WUPA` to read it again while it lies on the reader         |
| Random `BITERROR`                                 | card at the edge of the field, long or loose wires, noisy 3.3 V supply                  |
| `AUTH_FAILED` with the right key                  | wrong key type — Key A and Key B are different keys |
| Key B: authentication passes, the operation fails | on a new card (access bits `FF 07 80`) Key B is *readable*, and a readable Key B can't be used — the card refuses every memory operation after it. Use Key A ([details](../README.md#sector-trailer)) |
| `OP_NOT_POSSIBLE` on write, read works            | the access bits allow reading but not writing with this key                             |
| `OP_NOT_POSSIBLE` on increment                    | the block isn't a value block yet — write it in value block format first               |
| `OP_NOT_POSSIBLE` on every read                   | the card isn't MIFARE Classic (Ultralight, NTAG, DESFire, phone) — only UID works       |

---

## 🧩 Types

`#include "rc522/enums.hpp"`

```cpp
struct Uid {
    uint8_t bytes[10];
    uint8_t size;          // 4, 7 or 10
};

struct Read_Block {
    uint8_t bytes[16];
    uint8_t size;          // 16 after a successful read
};
```

| Enum                     | Values                                                  |
| ------------------------ | ------------------------------------------------------- |
| `key`                    | `KeyA`, `KeyB`                                          |
| `WAKING_CARD_UP_FOR_UID` | `REQA`, `WUPA`                                          |
| `ALTERATION_OP`          | `INCREMENT`, `DECREMENT`, `RESTORE`                     |
| `RFCfgReg_Gain`          | `DB_18`, `DB_23`, `DB_33`, `DB_38`, `DB_43`, `DB_48`    |
