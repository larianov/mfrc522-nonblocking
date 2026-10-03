#pragma once
#include "rc522/commands.hpp"
#include <array>
#include <cstdint>
namespace rc522 {

enum class result_of_op : uint8_t {
    DEVICE_IS_NOT_RESPONDING,
    WRONG_DEVICE,
    WRONG_SOFT,
    DEVICE_TIMEOUT,
    REGISTERES_NOT_CHANGING,
    SUCC,
};

enum class result_of_transaction : uint8_t {
    Temp_err,
    CollErr,
    CRCErr,
    ParityErr,
    ProtocolErr,
    Time_out,
    WAIT,
    SUCC,
};

enum class RFCfgReg_Gain : uint8_t {
    DB_18 = 0b000,
    DB_23 = 0b001,
    DB_33 = 0b100,
    DB_38 = 0b101,
    DB_43 = 0b110,
    DB_48 = 0b111,
};

struct Uid {
    uint8_t bytes[10];
    uint8_t size;
};


struct Read_Block{
    uint8_t bytes[16];
    uint8_t size;
};

enum class result_of_card : uint8_t {
    WAIT,
    SUCC,
    NO_CARD,
    COLLISION,
    BITERROR,
    TIMEOUT,
    OP_NOT_POSSIBLE,
    KEY_WAS_REJECTED,
    ERROR_FROM_IC,
    AUTH_FAILED,
};

enum class Selecting : uint8_t {
    START,
    WAIT_UID,
    WAIT_SAK,
};

enum class Halt_states : uint8_t {
    START,
    HALTING,
};

enum class TIMEOUT_LEVELS : uint8_t {
    Ti1,
    Ti5,
    Ti10,
};

enum class Uid_states : uint8_t {
    IDLE,
    RECIEVE_UID,
    HALT,
};


enum class Uid_activate : uint8_t{
    REQA_WUPA,
    ATQA,
    RESOLVE_UID,
};

enum class READING_STATES : uint8_t {
    IDLE,
    PREP_CARD_FOR_RW,
    READ_BOCK,
    HALT,
};

enum class key : uint8_t{
    KeyA,
    KeyB,
};

struct reading_un{
    READING_STATES rstate_;
    uint8_t block;
    std::array<uint8_t, 6> key_buff;
    key keyv;
};

enum class WRITING_STATES : uint8_t {
    IDLE,
    PREP_CARD_FOR_RW,
    WRITING_PT1,
    WRITING_PT2,
    HALT,
};

struct writing_un{
    WRITING_STATES rstate_;
    uint8_t block;
    std::array<uint8_t, 6> key_buff;
    key keyv;
    std::array<uint8_t, 16> write_buff;
};

enum class PREPARE_CARD_FOR_RW : uint8_t {
    SELECTING,
    AUTH_SENT,
    AUTH_WAIT,
};

enum class way_of_send : uint8_t{
    TRANSIEVE,
    MFAUNT,
};

enum class ALTERATION_OP : uint8_t{
    INCREMENT = 0xC1,
    DECREMENT = 0xC0,
    RESTORE = 0xC2,
};

enum class ALTERATION_STATE : uint8_t{
    IDLE,
    PREP_CARD_FOR_RW,
    WRITING_PT1,
    WRITING_PT2,
    TRANSFER,
    HALT,
};

struct alteration_un{
    ALTERATION_OP op;
    ALTERATION_STATE rstate_;
    uint8_t block_src;
    uint8_t block_dst;
    std::array<uint8_t, 6> key_buff;
    int32_t operand;
    key keyv;
};


} // namespace rc522