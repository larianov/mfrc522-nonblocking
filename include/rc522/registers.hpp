#include <cstdint>
inline constexpr uint8_t CommandReg = (0x01);
inline constexpr uint8_t CommandReg_W = (CommandReg << 1);
inline constexpr uint8_t CommandReg_R = (CommandReg << 1) | (1u << 7);
inline constexpr uint8_t VersionReg = (0x37);
inline constexpr uint8_t VersionReg_R = (VersionReg << 1) | (1u << 7);