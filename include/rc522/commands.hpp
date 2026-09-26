#pragma once
#include <cstdint>
inline constexpr uint8_t CMD_SOFT_RESET = 0b0000'1111;
inline constexpr uint8_t POWER_DOWN = (1U << 4U); // soft power-down mode entered on 1
inline constexpr uint8_t SET_UP_TIMER = (0b1'000'1111); // set-up automatic timer, and set 4 highest bits of tprescaler to 1
inline constexpr uint8_t SET_UP_TPRESCALER_LO = (0xFF); // set-up lowest bits of tprescaler to 1