#pragma once

#include <cstdint>

namespace rc522 {
inline constexpr uint8_t CommandReg = 0x01;
inline constexpr uint8_t VersionReg = 0x37;
inline constexpr uint8_t TModeReg = 0x2A;
inline constexpr uint8_t TPrescalerReg = 0x2B;
inline constexpr uint8_t ModeReg = 0x11;
inline constexpr uint8_t TxModeReg = 0x12;
inline constexpr uint8_t RxModeReg = 0x13;
inline constexpr uint8_t TxControlReg = 0x14;
inline constexpr uint8_t TxASKReg = 0x15;
inline constexpr uint8_t RFCfgReg = 0x26;
inline constexpr uint8_t FIFOLevelReg = 0x0A;
inline constexpr uint8_t ComIrqReg = 0x04;
inline constexpr uint8_t DivIrqReg = 0x05;
inline constexpr uint8_t FIFODataReg = 0x09;
inline constexpr uint8_t BitFramingReg = 0x0D;
inline constexpr uint8_t ErrorReg = 0x06;
inline constexpr uint8_t TReloadVal_Hi = 0x2C;
inline constexpr uint8_t TReloadVal_Lo = 0x2D;
inline constexpr uint8_t Status2Reg = 0x08;
inline constexpr uint8_t ComIEnReg = 0x02;
} // namespace rc522