#pragma once
#include <cstdint>
namespace rc522 {
inline constexpr uint8_t NVB_SEl_CODE = 0x70;
inline constexpr uint8_t NVB_COl_CODE = 0x20;
inline constexpr uint8_t CT_CODE = 0x88;
inline constexpr uint8_t REQA_TRANS = 0x26;
inline constexpr uint8_t WUPA_TRANS = 0x52;
inline constexpr uint8_t codes_of_cascades[3] = {0x93, 0x95, 0x97};
inline constexpr uint8_t CMD_FOR_CARD_HALT[2] = {0x50, 0x00};
inline constexpr uint8_t CMD_AUTH_CODE[2] = {0x60, 0x61};
} // namespace rc522