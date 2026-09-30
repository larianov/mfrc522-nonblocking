#pragma once
#include "rc522/commands.hpp"
#include <cstdint>
namespace rc522 {

enum class result_of_op : uint8_t {
    DEVICE_IS_NOT_RESPONDING,
    WRONG_DEVICE,
    WRONG_SOFT,
    DEVICE_TIMEOUT,
    REGISTERES_NOT_CHANGING,
    SUCC
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
    DB_48 = 0b111
};

struct Uid {
    uint8_t bytes[10];
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
    ERROR_FROM_IC,
};

enum class Selecting : uint8_t {
    START,
    WAIT_UID,
    WAIT_SAK
};

enum class Halt_states : uint8_t {
    START,
    HALTING
};

enum class TIMEOUT_LEVELS : uint8_t {
    Ti1,
    Ti5,
    Ti10
};

enum class Uid_states : uint8_t {
    RECIEVE_UID,
    HALT
};


enum class Uid_activate : uint8_t{
    REQA,
    ATQA,
    RESOLVE_UID
};

enum class READING_STATES : uint8_t {
    WUPA,
    ATQA,
    RESOLVE_UID,
    AUTH,
    READ_BOCK,
    HALT
};

enum class WRITING_STATES : uint8_t {
    WUPA,
    ATQA,
    RESOLVE_UID,
    AUTH,
    WRITING_BOCK,
    HALT
};

} // namespace rc522