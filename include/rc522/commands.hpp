#pragma once
#include <cstdint>
#include <sys/_intsup.h>
namespace rc522 {

inline constexpr uint8_t CMD_SOFT_RESET = 0b0000'1111;
inline constexpr uint8_t POWER_DOWN = (1U << 4U);             // soft power-down mode entered on 1
inline constexpr uint8_t POWER_WAKE_UP = 0b0000'0111;         // wake up
inline constexpr uint8_t POWER_SET_POWER_DOWN = 0b000'1'0111; // power down
inline constexpr uint8_t SET_UP_TIMER =
    (0b1'000'1111); // set-up automatic timer, and set 4 highest bits of tprescaler to 1
inline constexpr uint8_t SET_UP_TPRESCALER_LO = (0xFF);       // set-up lowest bits of tprescaler to 1
inline constexpr uint8_t SET_UP_MOD_REG = 0b0011'1101;        // set-up calccrc by standard of 14443A
inline constexpr uint8_t SET_UP_TX_CONTROL_REG = 0x43; // set-uping TxControlReg to use both, tx1 and tx2, and
                                                              // make field with power of difference between this 2
inline constexpr uint8_t SET_UP_FORCE_ASK =
    (1U << 6U); // set-up TxAskREG to force 100% ask, what means during modulation pauses while transmitting to the card
inline constexpr uint8_t FFLUSH_FIFO = (1U << 7U);
inline constexpr uint8_t CMD_IDLE = (0U);
inline constexpr uint8_t CLEAR_M_BITS_ComIrqReg = 0b0'1111'111;
inline constexpr uint8_t CLEAR_M_BITS_DivIrqReg = 0b0'1111'111;
inline constexpr uint8_t CMD_Transceive = 0b0000'1100;
inline constexpr uint8_t START_TRANSMISSION_FOR_REQA = 0b1'0000'111;
inline constexpr uint8_t START_TRANSMISSION_FULL_FOR_MFAUT = 0x00;
inline constexpr uint8_t CMD_MFAuthent = 0b0000'1110;
inline constexpr uint8_t START_TRANSMISSION_FULL = 0b1'0000'000;
inline constexpr uint8_t CL_RESOLVER_1 = 0x93;
inline constexpr uint8_t CL_RESOLVER_2 = 0x20;
inline constexpr uint8_t SET_CRC_ON_TX = (1U << 7U);
inline constexpr uint8_t SET_CRC_ON_RX = (1U << 7U);
inline constexpr uint8_t SET_UP_TRELOAD_LO_FOR_10MS = (1U << 4U);
inline constexpr uint8_t SET_UP_TRELOAD_LO_FOR_1MS = (1U);
inline constexpr uint8_t SET_UP_TRELOAD_LO_FOR_5MS = (1U << 3U);
inline constexpr uint8_t SET_UP_TRELOAD_LO_FOR_0_5MS = 1U;
inline constexpr uint8_t IS_MFCrypto1On = (1U << 3U);
} // namespace rc522