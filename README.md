<div align="center">

# 📡 RC522 Driver

**A non-blocking, platform-independent C++17 driver for the MFRC522 RFID reader —
read UIDs from any ISO 14443A card and read, write, increment, decrement and
restore blocks on MIFARE Classic cards.**

[![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-any-blueviolet)](docs/API.md#-porting-to-your-platform)
[![Port](https://img.shields.io/badge/port%20included-RP2040-green)](include/rc522/platform/pico/spi_transport.hpp)
[![Chip](https://img.shields.io/badge/chip-MFRC522-orange)](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf)
[![License](https://img.shields.io/badge/license-MIT-lightgrey)](LICENSE)

</div>

---

## ✨ Features

- **Non-blocking** — every operation is a state machine. You start it and call
  `poll()` whenever you have time; the driver never waits for the card, so the
  CPU is free for the rest of your program
- **Platform-independent** — the driver doesn't know what MCU it runs on. To port
  it, you write **one class with three methods**. A ready-made port for the
  Raspberry Pi Pico is included
- **Optional IRQ mode** — the RC522 `IRQ` pin tells you when to call `poll()`, so
  the MCU can sleep in between
- **UID + SAK** from any ISO 14443A card, with 4, 7 and 10 byte UIDs
- **MIFARE Classic Mini / 1K / 4K** — read and write blocks with Key A or Key B
- **Value blocks** — increment, decrement and restore
- **Can't brick a card by accident** — block 0 is never written, sector trailers
  only on explicit request and only with valid access bits
- **Small and allocation-free** — ~3.5 KB of flash, 220 bytes of RAM for both
  objects, no `new`, no exceptions, no RTTI (see [Footprint](#-footprint))
- **Tested on real hardware** — a test program runs every operation against a
  real card, no mocks

---

## 🔌 Platform Independence

The driver talks to the chip through one small interface, `Transport`. That's the
only place where the platform shows up:

| Method       | What your platform does                                  |
| ------------ | -------------------------------------------------------- |
| `transfer`   | one SPI transfer: CS low → send / receive bytes → CS high |
| `delayUs`    | wait some microseconds                                   |
| `microus_32` | return a microsecond counter                             |

Implement these three methods with your SDK — STM32 HAL, ESP-IDF, Zephyr,
Linux `spidev`, anything with SPI — and the whole driver works on it:

```cpp
class MyTransport : public rc522::Transport {
  public:
    void transfer(const uint8_t *tx, uint8_t *rx, std::size_t len) override { /* CS low, SPI, CS high */ }
    void delayUs(uint32_t us) override { /* wait */ }
    uint32_t microus_32() override { /* µs counter */ }
};

MyTransport transport;
rc522::Rc522 rc{transport};      // from here on — the same code on every platform
```
 Nothing
else needs to change: no `#ifdef`s, no config headers, no build flags.

The core library (`rc522`) has no dependencies besides the C++ standard library.
The Pico port (`PicoTransport`, target `rc522_pico`) is about 20 lines and is a
good reference for writing your own.

👉 How to write one: [Porting to your platform](docs/API.md#-porting-to-your-platform)

---

## 🧠 How It Works

The driver has three layers:

```
CardReader  ──▶  Rc522  ──▶  Transport  ──▶  SPI
(card logic)    (chip)      (your platform)
```

- **`CardReader`** — the card protocol (ISO 14443A + MIFARE Classic) as state
  machines. This is what you use
- **`Rc522`** — the chip: init, registers, FIFO, starting a transfer to the card
  and checking whether it finished
- **`Transport`** — your platform

### Start → poll → result

Every card operation is used the same way:

1. **Start** it — the driver checks the arguments and remembers the operation.
   Nothing is sent yet
2. **Poll** it — each `poll()` moves the operation forward until it has to wait
   for the card. It returns `WAIT` while the operation is running and the final
   result when it's done
3. **Take the result** — UID, SAK or block data

```cpp
reader.start_read_transaction(4, key, rc522::key::KeyA);
rc522::result_of_card res{rc522::result_of_card::WAIT};
while (res == rc522::result_of_card::WAIT) {
    res = reader.poll();         // your other work fits between calls
}
auto data = reader.block();
```

Without IRQ you call `poll()` in your main loop. With IRQ you call it after each
interrupt from the chip.

### What happens inside one operation

```
UID:    REQA/WUPA ─▶ ATQA ─▶ anticollision + SELECT (cascade 1–3) ─▶ HALT
READ:   WUPA ─▶ ATQA ─▶ SELECT ─▶ AUTH ─▶ READ ─────────────────────────────▶ HALT
WRITE:  WUPA ─▶ ATQA ─▶ SELECT ─▶ AUTH ─▶ WRITE ─ACK─▶ 16 bytes ─ACK────────▶ HALT
VALUE:  WUPA ─▶ ATQA ─▶ SELECT ─▶ AUTH ─▶ INC/DEC/RESTORE ─ACK─▶ operand ─▶ TRANSFER ─ACK─▶ HALT
```

Every arrow is one exchange with the card. Each one is guarded by the chip's
timer (~1, 5 or 10 ms), so a card that disappears gives `TIMEOUT` instead of a
hang.

---

## 📦 Footprint

Measured on Cortex-M0+ (RP2040), `arm-none-eabi-gcc` 16.2, release build
(`-DNDEBUG -fno-exceptions -fno-rtti`):

| What                  | Size                                    |
| --------------------- | --------------------------------------- |
| Flash, `-Os`          | ~3.5 KB (`Rc522` 1.3 KB + `CardReader` 2.2 KB) |
| Flash, `-O2`          | ~5.0 KB                                 |
| RAM, `Rc522`          | 140 bytes (mostly the FIFO buffer)      |
| RAM, `CardReader`     | 80 bytes                                |
| Static / global data  | none                                    |
| Heap                  | none                                    |

- **No `new` / `malloc`** — all state lives inside the two objects you create;
  put them wherever you want (globals, stack, a static pool)
- **No exceptions, no RTTI** — every error is a return value; the library builds
  and links with `-fno-exceptions -fno-rtti` and needs only `memcpy`, `memset`
  and `abort` from the runtime
- **Small stack use** — the largest local buffer is 12 bytes

> A debug build keeps the internal `assert` checks and is ~300 bytes bigger.

---

## 💳 Supported Cards

| Operation                  | Cards                                                                    |
| -------------------------- | ------------------------------------------------------------------------ |
| UID + SAK                  | any ISO 14443A card: MIFARE Classic, Ultralight, NTAG, DESFire, phones…  |
| Read / write / value block | MIFARE Classic Mini (SAK `09`), 1K (`08`), 4K (`18`), including clones   |

Memory operations use MIFARE Classic authentication (Crypto1), so other card
types are refused with `OP_NOT_POSSIBLE`.

---

## 🗂 MIFARE Classic Memory

> Based on the NXP datasheet [MF1S50YYX_V1](https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf)
> (MIFARE Classic EV1 1K), sections 8.6–8.7.

### Sectors and blocks

A **1K** card has 1024 bytes: **16 sectors × 4 blocks × 16 bytes**. The driver
addresses blocks by their absolute number: `block = sector × 4 + index`.

| Sector | Blocks  | Block 0 of sector  | Blocks 1–2 | Last block         |
| :----: | :-----: | ------------------ | ---------- | ------------------ |
| 0      | 0–3     | **manufacturer**   | data       | **sector trailer** |
| 1      | 4–7     | data               | data       | **sector trailer** |
| …      | …       | data               | data       | **sector trailer** |
| 15     | 60–63   | data               | data       | **sector trailer** |

- **Block 0** — UID and manufacturer data, write-protected at the factory.
  That's why examples and tests use block 4: the first free data block
- **Data blocks** — 16 bytes each, content at delivery is undefined
- **Sector trailer** — the last block of every sector: keys and access bits
- **Every operation needs authentication** of the block's sector with Key A or
  Key B — the driver does this for you in every operation

Other sizes, as handled by the driver: **Mini** — 5 sectors (blocks 0–19).
**4K** — sectors 0–31 have 4 blocks (0–127), sectors 32–39 have 16 blocks
(128–255); the trailer is always the last block of the sector.

### Sector trailer

| Bytes | 0–5   | 6–8         | 9         | 10–15                   |
| ----- | ----- | ----------- | --------- | ----------------------- |
|       | Key A | Access bits | user byte | Key B (or data)         |

- Keys are **never readable** — reading a trailer returns zeros in place of
  Key A (and Key B, unless the access bits make it readable)
- **At delivery** all keys are `FF FF FF FF FF FF` and the access bits are
  `FF 07 80` — the *transport configuration*: Key A can do everything
- In the transport configuration **Key B is readable, and a readable Key B can't
  be used for authentication** — the card refuses every memory operation after it.
  **Use Key A on new cards**

### Value blocks

A data block becomes a value block by **writing it in value block format**. The
value is a signed 32-bit integer, stored three times; the address byte four times:

| Bytes | 0–3   | 4–7      | 8–11  | 12    | 13     | 14    | 15     |
| ----- | ----- | -------- | ----- | ----- | ------ | ----- | ------ |
|       | value | ~value   | value | addr  | ~addr  | addr  | ~addr  |

Little endian, negative values in two's complement. Example from the datasheet —
value 1234567, address 17:

```
87 D6 12 00  78 29 ED FF  87 D6 12 00  11 EE 11 EE
```

Increment / decrement / restore put the result into the card's internal transfer
buffer, and **transfer** writes it into a block. The driver always does both
steps in one operation, from `block_src` to `block_dst`. The address byte is
never changed by these operations — only by a write.

---

## 🔐 Writing a Sector Trailer Safely

Writing a trailer changes the keys and the access rights of the whole sector.
Two ways to lose a sector **forever**:

1. **Invalid access bits.** Every access bit is stored twice, normal and inverted.
   If the card finds them inconsistent, *the whole sector is irreversibly blocked*
2. **Valid, but locking access bits** — e.g. a configuration where no key may
   write the trailer anymore, or a key you didn't write down

The driver protects against the first one: it writes a trailer only with
`REQUIRED = true` and refuses access bits that aren't consistent. **The second
one is up to you** — the driver can't know what you intended.

### How access bits work

Each block of the sector has 3 bits `C1 C2 C3` (blocks 0, 1, 2 and the trailer
itself = 3). They're packed into bytes 6–8, each bit once normal and once
inverted (`~`):

| Byte | Bits 7–4            | Bits 3–0            |
| ---- | ------------------- | ------------------- |
| 6    | ~C2 (blocks 3…0)    | ~C1 (blocks 3…0)    |
| 7    | C1 (blocks 3…0)     | ~C3 (blocks 3…0)    |
| 8    | C3 (blocks 3…0)     | C2 (blocks 3…0)     |

What the bits mean for **data blocks** (datasheet table 8):

| C1 C2 C3 | Read  | Write | Increment | Decrement, restore | Use              |
| :------: | :---: | :---: | :-------: | :----------------: | ---------------- |
| `0 0 0`  | A\|B  | A\|B  | A\|B      | A\|B               | transport config |
| `0 1 0`  | A\|B  | —     | —         | —                  | read-only        |
| `1 0 0`  | A\|B  | B     | —         | —                  | read/write       |
| `1 1 0`  | A\|B  | B     | B         | A\|B               | value block      |
| `0 0 1`  | A\|B  | —     | —         | A\|B               | value, decrement only |
| `0 1 1`  | B     | B     | —         | —                  | read/write       |
| `1 0 1`  | B     | —     | —         | —                  | read-only        |
| `1 1 1`  | —     | —     | —         | —                  | locked           |

And for the **sector trailer** (datasheet table 7):

| C1 C2 C3 | Key A write | Access bits read / write | Key B read / write | Note                      |
| :------: | :---------: | :----------------------: | :----------------: | ------------------------- |
| `0 0 0`  | A           | A / —                    | A / A              | Key B readable            |
| `0 1 0`  | —           | A / —                    | A / —              | Key B readable            |
| `1 0 0`  | B           | A\|B / —                 | — / B              |                           |
| `1 1 0`  | —           | A\|B / —                 | — / —              | **frozen**                |
| `0 0 1`  | A           | A / A                    | A / A              | Key B readable, transport |
| `0 1 1`  | B           | A\|B / B                 | — / B              |                           |
| `1 0 1`  | —           | A\|B / B                 | — / —              |                           |
| `1 1 1`  | —           | A\|B / —                 | — / —              | **frozen**                |

Key A is never readable. "Frozen" means nothing in the trailer can be changed
ever again.

### Ready-made access bits

| Bytes 6–8  | Data blocks          | Trailer                       | For                              |
| ---------- | -------------------- | ----------------------------- | -------------------------------- |
| `FF 07 80` | `000` — A\|B do all  | `001` — Key A manages it      | transport config, factory default |
| `78 77 88` | `100` — read A\|B, write B | `011` — Key B manages it | read-mostly data, Key B is the admin key |
| `08 77 8F` | `110` — value block: decrement A\|B, increment B | `011` — Key B manages it | wallet / counter: Key A can only spend |

### Checklist

1. **Practice on a sector of a test card** before touching real ones
   bits is the safest possible change
2. **Write the full 16 bytes:** Key A, access bits, byte 9 (free user byte), Key B.
   A trailer read returns zeros instead of keys, so never write back what you read
3. **Write down the keys** before writing — a lost key is a lost sector
4. **Check right after writing** — read a data block of that sector with the new key

---

## 🚀 Getting Started

1. **Add the library** to your CMake project with `add_subdirectory` and link
   `rc522` (or `rc522_pico` on the Pico)
2. **Give it a transport** — use `PicoTransport` or write your own
3. **Init the chip** — `Rc522::init()`, with or without IRQ
4. **Run operations** through `CardReader` — start, poll, take the result

The full API with code: [`docs/API.md`](docs/API.md).
Working programs: [`examples/`](examples).

---

## 📚 Examples

Ready-to-flash programs for the Raspberry Pi Pico, each showing one thing:

| Example                              | Shows                                                         |
| ------------------------------------ | ------------------------------------------------------------- |
| [`uid`](examples/uid/main.cpp)       | reading UID + SAK in a loop while the card stays on the reader |
| [`read`](examples/read/main.cpp)     | reading a block                                               |
| [`write`](examples/write/main.cpp)   | writing a block                                               |
| [`multi`](examples/multi/main.cpp)   | making a value block, then incrementing it and reading it back |
| [`irq`](examples/irq/main.cpp)       | IRQ mode — the MCU sleeps until the chip has news             |

**Build and flash** one example with `picotool` (or copy `<name>.uf2` in
BOOTSEL mode). Output goes to USB serial.

```bash
cd examples
cmake --preset pico
cmake --build ../build/examples --target load_uid
```

**Wiring** used by the examples (SPI0):

| RC522  | `SDA` (CS) | `SCK` | `MOSI` | `MISO` | `IRQ` | `RST` | `3.3V` | `GND` |
| ------ | :--------: | :---: | :----: | :----: | :---: | :---: | :----: | :---: |
| Pico   | `GP5`      | `GP2` | `GP3`  | `GP4`  | `GP6` | `3V3` | `3V3`  | `GND` |

`IRQ` is used only by the `irq` example. The RC522 runs at **3.3 V only**.

### Hardware test

[`tests/hw_test`](tests/hw_test/main.cpp) checks the driver against a real
**MIFARE Classic 1K** card: UID stability over 100 reads, write and read back,
wrong key, write guards, increment / decrement / restore, a busy reader and
`abort()`, and that a halted card stays silent. It prints `PASS` / `FAIL` for
each check. Built the same way as the examples, from `tests/`.

> ⚠️ The test **overwrites blocks 4, 5 and 6** of the card (default key
> `FF FF FF FF FF FF`). Use a card you don't need.

---

## ⚠️ Things to Keep in Mind

<details>
<summary><b>The card is halted after every operation</b></summary>

A halted card answers only to **WUPA**. Memory operations always use WUPA, so
they work on a card that stays on the reader. For UIDs you choose:

- **WUPA** — read the same card again and again while it lies on the reader
- **REQA** (default) — read each card **once**; it stays silent until it's taken
  away and brought back
</details>

<details>
<summary><b>Block 0 and sector trailers are protected</b></summary>

Block 0 holds the manufacturer data and is never written. Sector trailers hold
the keys and access bits — the driver writes them only when you explicitly ask
for it, and refuses if the access bits are inconsistent, because wrong access
bits lock a sector forever. See [Writing a Sector Trailer Safely](#-writing-a-sector-trailer-safely).
</details>

<details>
<summary><b>Value operations need a value block</b></summary>

Increment, decrement and restore work only on a block in value block format,
and the source and destination must be in the same sector. The format is in
[Value blocks](#value-blocks), and
[`examples/multi`](examples/multi/main.cpp) creates such a block.
</details>

<details>
<summary><b>One card in the field</b></summary>

Anticollision between several cards isn't implemented yet. Two cards at once
usually give `COLLISION`.
</details>

<details>
<summary><b>Only chip versions 0x91 and 0x92</b></summary>

`init()` refuses other versions with `WRONG_DEVICE` — some cheap clone chips
report a different one.
</details>

---

## 📂 Repository Structure

```
.
├── include/rc522/
│   ├── card_reader.hpp        # CardReader — card operations
│   ├── rc522.hpp              # Rc522 — the chip
│   ├── transport.hpp          # Transport — implement this for your platform
│   ├── enums.hpp              # results, keys, operations, Uid, Read_Block
│   ├── commands.hpp           # chip register values
│   ├── commands_for_card.hpp  # ISO 14443A / MIFARE commands
│   ├── registers.hpp          # chip register addresses
│   └── platform/pico/
│       └── spi_transport.hpp  # PicoTransport
├── src/                       # card_reader.cpp, rc522.cpp
├── examples/                  # uid, read, write, multi, irq
├── tests/hw_test/             # test on real hardware
└── docs/API.md                # API reference + porting guide
```

---

## 🗺 Roadmap

- [x] UID + SAK, cascade levels 1–3
- [x] MIFARE Classic read / write
- [x] Value blocks: increment / decrement / restore
- [x] IRQ mode
- [ ] Anticollision — several cards in the field
- [ ] PIO-based transport for the RP2040

---

## 📜 License

Released under the MIT License. See [`LICENSE`](LICENSE) for details.
