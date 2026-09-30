#include "rc522/card_reader.hpp"
#include "rc522/commands_for_card.hpp"
#include "rc522/enums.hpp"
#include <array>
#include <cstdint>
#include <variant>
namespace rc522 {

result_of_card CardReader::convert_error(result_of_transaction err) {
    if (err == result_of_transaction::Time_out)
        return result_of_card::TIMEOUT;
    if (err == result_of_transaction::WAIT)
        return result_of_card::WAIT;
    if (err == result_of_transaction::CollErr)
        return result_of_card::COLLISION;
    if (err == result_of_transaction::ParityErr || err == result_of_transaction::CRCErr)
        return result_of_card::BITERROR;
    else
        return result_of_card::ERROR_FROM_IC;
}

result_of_card CardReader::step_select_fsm() {
    switch (select_.state_) {
    case Selecting::START: {
        buff_uid_sak = {};
        uid_unfull = false;
        ic_com.start_exc(std::array<uint8_t, 2>{codes_of_cascades[0], NVB_COl_CODE}.data(), 2, false, false,
                         TIMEOUT_LEVELS::Ti1);
        select_.layer_ = 1;
        select_.state_ = Selecting::WAIT_UID;
        break;
    }
    case Selecting::WAIT_UID: {
        uint8_t arr[5];
        auto result = ic_com.check_exc();
        if (result != result_of_transaction::SUCC) {
            return convert_error(result);
        }
        auto res = ic_com.recieve_exc(arr, 5);
        if (res != result_of_transaction::SUCC) {
            return result_of_card::BITERROR;
        }
        uint8_t BCC{};
        for (uint8_t i{}; i < 5; i++) {
            BCC ^= arr[i];
        }
        if (BCC != 0) {
            return result_of_card::BITERROR;
        }
        uint8_t starting_idx = 0;
        if (arr[0] == CT_CODE) {
            starting_idx = 1;
            uid_unfull = true;
        }
        for (auto i{starting_idx}; i < 4; i++) {
            if (buff_uid_sak.first.size > 9) {
                return result_of_card::BITERROR;
            }
            buff_uid_sak.first.bytes[buff_uid_sak.first.size++] = arr[i];
        }
        uint8_t retransmitting_arr[7];
        retransmitting_arr[0] = codes_of_cascades[select_.layer_ - 1];
        retransmitting_arr[1] = NVB_SEl_CODE;
        for (uint8_t i{2}; i < 7; i++) {
            retransmitting_arr[i] = arr[i - 2];
        }
        ic_com.start_exc(retransmitting_arr, 7, false, true, TIMEOUT_LEVELS::Ti1);
        select_.state_ = Selecting::WAIT_SAK;
        break;
    }
    case Selecting::WAIT_SAK: {
        uint8_t val;
        auto er_result = ic_com.check_exc();
        if (er_result != result_of_transaction::SUCC) {
            return convert_error(er_result);
        }
        auto result = ic_com.recieve_exc(&val, 1);
        if (result != result_of_transaction::SUCC) {
            return result_of_card::BITERROR;
        }
        if ((val & (1U << 2U)) == (1U << 2U)) {
            if (!uid_unfull) {
                return result_of_card::BITERROR;
            }
            select_.layer_++;
            if (select_.layer_ > 3) {
                return result_of_card::BITERROR;
            }
            select_.state_ = Selecting::WAIT_UID;
            uint8_t arr[2];
            arr[0] = codes_of_cascades[select_.layer_ - 1];
            arr[1] = NVB_COl_CODE;
            uid_unfull = false;
            ic_com.start_exc(arr, 2, false, false, TIMEOUT_LEVELS::Ti1);
        } else {
            if (uid_unfull)
                return result_of_card::BITERROR;
            buff_uid_sak.second = val;

            return result_of_card::SUCC;
        }
    }
    }
    return result_of_card::WAIT;
}

// reminder1!!!!!!!!
//  here is only one case where timeout is needed result!
result_of_card CardReader::halt() {
    switch (halt_st_) {
    case Halt_states::START:
        ic_com.start_exc(CMD_FOR_CARD_HALT, 2, false, false, TIMEOUT_LEVELS::Ti1);
        halt_st_ = Halt_states::HALTING;
        break;
    case Halt_states::HALTING:
        {
        auto result = ic_com.check_exc();
        if (result == result_of_transaction::WAIT) {
            break;
        }
        halt_st_ = Halt_states::START;
        return convert_error(result);
        }
    }
    return result_of_card::WAIT;
}

result_of_card CardReader::step_activating_card(){
    switch (state_of_activating_uid_) {
        case Uid_activate::REQA:
            ic_com.start_exc(&REQA_TRANS, 1, true, false, TIMEOUT_LEVELS::Ti1);
            state_of_activating_uid_ = Uid_activate::ATQA;
            break;
        case Uid_activate::ATQA: {
            auto res = ic_com.check_exc();
            if (res != result_of_transaction::SUCC) {
                auto res_err = convert_error(res);
                if (res_err == result_of_card::WAIT)
                    break;
                return res_err;
            }
            uint8_t temp_buf[2];
            res = ic_com.recieve_exc(temp_buf, 2);
            if (res != result_of_transaction::SUCC) {
                auto res_err = convert_error(res);
                return result_of_card::BITERROR;
            }
            state_of_activating_uid_ = Uid_activate::RESOLVE_UID;
            break;
        }
        case rc522::Uid_activate::RESOLVE_UID: {
            auto res = step_select_fsm();
            if (res == result_of_card::WAIT)
                break;
            if (res == result_of_card::SUCC) {
                state_of_activating_uid_ = Uid_activate::REQA;
                return result_of_card::SUCC;
            } else {
                return res;
            }
        }
    }
    return result_of_card::WAIT;
}

std::pair<Uid, result_of_card> CardReader::get_uid() {
    if (std::holds_alternative<std::monostate>(op)) {
        op = Uid_states::RECIEVE_UID;
        state_of_activating_uid_ = {};
        select_ = {};
    }
    if (!std::holds_alternative<Uid_states>(op)) {
        return {buff_uid_sak.first, result_of_card::OP_NOT_POSSIBLE};
    }
    auto &st = std::get<Uid_states>(op);
    switch (st) {
        case Uid_states::RECIEVE_UID:
        {
            auto result = step_activating_card();
            if (result == result_of_card::SUCC) 
                st = Uid_states::HALT;
            if (result == result_of_card::WAIT || result == result_of_card::SUCC) break;
            else{
                op = std::monostate();
                return {buff_uid_sak.first, result};
            }
        }
        case Uid_states::HALT: {
            auto res = halt();
            if (res == result_of_card::WAIT)
                break;
            if (res == result_of_card::TIMEOUT){
                op = std::monostate();
                return {buff_uid_sak.first, result_of_card::SUCC};
            }
            else{
                op = std::monostate();
                return {buff_uid_sak.first, res};
            }
        }
    }
    return {buff_uid_sak.first, result_of_card::WAIT};
}

 }// namespace rc522