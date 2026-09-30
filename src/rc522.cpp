#include "rc522/enums.hpp"
#include "rc522/rc522.hpp"
#include "rc522/commands.hpp"
#include "rc522/registers.hpp"
#include "rc522/transport.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <utility>
namespace rc522 {
Rc522::Rc522(Transport &t) : t_(t) {};

void Rc522::write_one_byte(uint8_t address, uint8_t value_to_write) {
    assert(address < 0x3C);
    uint8_t write_placeholder[2] = {static_cast<uint8_t>((address << 1U)), value_to_write};
    t_.transfer(write_placeholder, garbage, 2);
}

// in case of uncuccsess return 0xFF
uint8_t Rc522::read_one_byte(uint8_t address) {
    assert(address < 0x3C);
    uint8_t read_placeholder[2] = {0xFF, 0xFF};
    uint8_t write_placeholder[2] = {static_cast<uint8_t>(static_cast<uint8_t>(address << 1U) | (1U << 7U)), 0x00};
    t_.transfer(write_placeholder, read_placeholder, 2);
    return read_placeholder[1];
}

void Rc522::read_n_bytes(uint8_t *buff, uint8_t address, uint16_t N) {
    assert(address < 0x3C);
    assert(N < 65);
    for (uint8_t i{}; i < N; i++) {
        fifo_buff[i] = static_cast<uint8_t>(static_cast<uint8_t>(address << 1U) | (1U << 7U));
    }
    fifo_buff[N] = 0;
    t_.transfer(fifo_buff, fifo_buff, N + 1);
    for (uint8_t i{1}; i <= N; i++) {
        buff[i - 1] = fifo_buff[i];
    }
}

result_of_op Rc522::set_power_state(uint8_t power_up) {
    uint8_t status = read_one_byte(CommandReg);
    status = !(status & (1U << 4U));
    if (status == power_up)
        return result_of_op::SUCC;
    uint8_t attempts = 0;
    if (power_up == 1) {
        write_one_byte(CommandReg, POWER_WAKE_UP);
        while (attempts < 8) {
            auto result = read_one_byte(CommandReg);
            result = (result & POWER_DOWN);
            if (result == 0) {
                break;
            }
            attempts++;
            t_.delayUs(50);
        }
    } else {
        write_one_byte(CommandReg, POWER_SET_POWER_DOWN);
        while (attempts < 8) {
            auto result = read_one_byte(CommandReg);
            result = (result & POWER_DOWN);
            if (result == POWER_DOWN) {
                break;
            }
            attempts++;
            t_.delayUs(50);
        }
    }
    if (attempts >= 8)
        return result_of_op::DEVICE_TIMEOUT;
    return result_of_op::SUCC;
}

result_of_op Rc522::init() {
    write_one_byte(CommandReg, CMD_SOFT_RESET);
    uint8_t attempts{};
    while (attempts < 6) {
        auto result = read_one_byte(CommandReg);
        result = (result & POWER_DOWN);
        if (result == 0) {
            break;
        }
        attempts++;
        t_.delayUs(50);
    }
    if (attempts >= 6)
        return result_of_op::DEVICE_TIMEOUT;
    version_ = read_one_byte(VersionReg);
    if (version_ != 0x92 && version_ != 0x91) {
        return result_of_op::WRONG_DEVICE;
    }
    write_one_byte(TModeReg, SET_UP_TIMER);
    write_one_byte(TPrescalerReg, SET_UP_TPRESCALER_LO);
    write_one_byte(TReloadVal_Lo, SET_UP_TRELOAD_LO_FOR_1MS);
    if (read_one_byte(TModeReg) != SET_UP_TIMER)
        return result_of_op::REGISTERES_NOT_CHANGING;
    if (read_one_byte(TPrescalerReg) != SET_UP_TPRESCALER_LO)
        return result_of_op::REGISTERES_NOT_CHANGING;
    write_one_byte(ModeReg, SET_UP_MOD_REG);
    if (read_one_byte(ModeReg) != SET_UP_MOD_REG)
        return result_of_op::REGISTERES_NOT_CHANGING;
    // from this moment already 3 writings was succssessfull, so from writer side here can not be a problem
    write_one_byte(TxASKReg, SET_UP_FORCE_ASK);
    write_one_byte(TxControlReg, SET_UP_TX_CONTROL_REG);
    return result_of_op::SUCC;
}

const char *Rc522::version(uint8_t &version_mut) const {
    if (version_ == 0xFF) {
        version_mut = 0xFF;
        return "Version can't be determined untill init.";
    }
    version_mut = version_;
    if (version_ == 0x91)
        return "Version: 1.0";
    else if (version_ == 0x92)
        return "Version: 2.0";
    else
        return "Unknown version";
}

result_of_op Rc522::change_gain(RFCfgReg_Gain value) {
    auto RMW = read_one_byte(RFCfgReg);
    if (RMW == 0xFF)
        return result_of_op::DEVICE_IS_NOT_RESPONDING;
    RMW &= ~(0b111U << 4U);
    RMW |= static_cast<uint8_t>((static_cast<uint8_t>(value) << 4U));
    write_one_byte(RFCfgReg, RMW);
    RMW = read_one_byte(RFCfgReg);
    if ((RMW & (0b111U << 4U)) != (static_cast<uint8_t>(value) << 4U))
        return result_of_op::REGISTERES_NOT_CHANGING;
    t_.delayUs(6'000);
    return result_of_op::SUCC;
}

void Rc522::clear_status() {
    write_one_byte(CommandReg, CMD_IDLE);
    write_one_byte(FIFOLevelReg, FFLUSH_FIFO);
    write_one_byte(ComIrqReg, CLEAR_M_BITS_ComIrqReg);
    write_one_byte(DivIrqReg, CLEAR_M_BITS_DivIrqReg);
}

result_of_transaction Rc522::error_decoding() {
    uint8_t error_byte = read_one_byte(ErrorReg);
    if ((error_byte & (1U)) == 1U) {
        return result_of_transaction::ProtocolErr;
    } else if ((error_byte & (1U << 1U)) == (1U << 1U)) {
        return result_of_transaction::ParityErr;
    } else if ((error_byte & (1U << 2U)) == (1U << 2U)) {
        return result_of_transaction::CRCErr;
    } else if ((error_byte & (1U << 3U)) == (1U << 3U)) {
        return result_of_transaction::CollErr;
    } else if ((error_byte & (1U << 6U)) == (1U << 6U)) {
        return result_of_transaction::Temp_err;
    } else {
        return result_of_transaction::SUCC;
    }
}

void Rc522::start_exc(const uint8_t *arr, uint16_t size, bool byt7e, bool crc, TIMEOUT_LEVELS time_levels) {
    clear_status();
    if (time_levels != last_time_) {
        switch (time_levels) {
        case TIMEOUT_LEVELS::Ti10:
            write_one_byte(TReloadVal_Lo, SET_UP_TRELOAD_LO_FOR_10MS);
            break;
        case TIMEOUT_LEVELS::Ti1:
            write_one_byte(TReloadVal_Lo, SET_UP_TRELOAD_LO_FOR_1MS);
            break;
        case TIMEOUT_LEVELS::Ti5:
            write_one_byte(TReloadVal_Lo, SET_UP_TRELOAD_LO_FOR_5MS);
            break;
        }
    }
    last_time_ = time_levels;
    if (crc) {
        write_one_byte(TxModeReg, SET_CRC_ON_TX);
        write_one_byte(RxModeReg, SET_CRC_ON_RX);
    } else {
        write_one_byte(TxModeReg, 0);
        write_one_byte(RxModeReg, 0);
    }

    for (uint16_t i{}; i < size; i++) {
        write_one_byte(FIFODataReg, arr[i]);
    }
    write_one_byte(CommandReg, CMD_Transceive);
    if (!byt7e)
        write_one_byte(BitFramingReg, START_TRANSMISSION_FULL);
    else
        write_one_byte(BitFramingReg, START_TRANSMISSION_FOR_REQA);
}

result_of_transaction Rc522::check_exc() {
    uint8_t byte = read_one_byte(ComIrqReg);
    if ((byte & (1U << 1U)) == (1U << 1U)) {
        return error_decoding();
    } else if ((byte & (1U << 5U)) == (1U << 5U)) {
        return result_of_transaction::SUCC;
    } else if ((byte & 1U) == 1U) {
        return result_of_transaction::Time_out;
     }
    return result_of_transaction::WAIT;
}

result_of_transaction Rc522::recieve_exc(uint8_t *arr, uint8_t size) {
    uint8_t byte = read_one_byte(FIFOLevelReg);
    if (byte != size) {
        return result_of_transaction::ProtocolErr;
    } else {
        read_n_bytes(arr, FIFODataReg, size);
    }
    return result_of_transaction::SUCC;
}
} // namespace rc522