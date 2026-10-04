#include "rc522/card_reader.hpp"
#include "rc522/commands.hpp"
#include "rc522/commands_for_card.hpp"
#include "rc522/enums.hpp"
#include "rc522/rc522.hpp"
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
        uint8_t val{};
        auto er_result = ic_com.check_exc();
        if (er_result != result_of_transaction::SUCC) {
            return convert_error(er_result);
        }
        auto result = ic_com.recieve_exc(&val, 1);
        if (result != result_of_transaction::SUCC) {
            return result_of_card::BITERROR;
        }
        if ((val & SAK_UID_NOT_COMPLETE) == SAK_UID_NOT_COMPLETE) {
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
        ic_com.start_exc(CMD_FOR_CARD_HALT, 2, false, true, TIMEOUT_LEVELS::Ti1);
        halt_st_ = Halt_states::HALTING;
        break;
    case Halt_states::HALTING:
        {
            auto result = ic_com.check_exc();
            if (result == result_of_transaction::WAIT) {
                break;
            }
            halt_st_ = Halt_states::START;
            auto res = convert_error(result);
            if (res == result_of_card::TIMEOUT) {
                res = result_of_card::SUCC;
            }
            return res;
        }
    }
    return result_of_card::WAIT;
}

result_of_card CardReader::step_activating_card(bool wupa){
    switch (state_of_activating_card) {
        case rc522::CARD_activate::REQA_WUPA:
            if(!wupa)ic_com.start_exc(&REQA_TRANS, 1, true, false, TIMEOUT_LEVELS::Ti1);
            else ic_com.start_exc(&WUPA_TRANS, 1, true, false, TIMEOUT_LEVELS::Ti1);
            state_of_activating_card = CARD_activate::ATQA;
            break;
        case rc522::CARD_activate::ATQA: {
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
            state_of_activating_card = CARD_activate::RESOLVE_UID;
            break;
        }
        case rc522::CARD_activate::RESOLVE_UID: {
            auto res = step_select_fsm();
            if (res == result_of_card::WAIT)
                break;
            if (res == result_of_card::SUCC) {
                state_of_activating_card = CARD_activate::REQA_WUPA;
                return result_of_card::SUCC;
            } else {
                return res;
            }
        }
    }
    return result_of_card::WAIT;
}

result_of_card CardReader::get_uid() {
    auto &st = std::get<uid_un>(op);
    switch (st.rstates) {
        case Uid_states::IDLE:
        {
            begin_op();
            st.rstates = Uid_states::RECIEVE_UID;
            break;
        }
        case Uid_states::RECIEVE_UID:
        {
            auto result = step_activating_card(static_cast<bool>(st.forcing_wake_up));
            if (result == result_of_card::SUCC) 
                st.rstates = Uid_states::HALT;
            if (result == result_of_card::WAIT || result == result_of_card::SUCC) break;
            else{
                return result;
            }
        }
        case Uid_states::HALT: {
            return halt();
        }
    }
    return result_of_card::WAIT;
}

void CardReader::abort(){
    op = std::monostate();
}

void CardReader::begin_op(){
    ic_com.clear_mauth();
    ic_com.clear_isr_flag();
    state_of_activating_card = {};
    select_ = {};
    prep_st = {};
    halt_st_ = {};   
}

result_of_card CardReader::start_uid_transaction(WAKING_CARD_UP_FOR_UID wc){
    if (!std::holds_alternative<std::monostate>(op)) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    op = uid_un{Uid_states::IDLE, wc};
    return result_of_card::SUCC;
}

result_of_card CardReader::start_read_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv){
    if (!std::holds_alternative<std::monostate>(op)) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    op = reading_un{READING_STATES::IDLE, block, keybuff, keyv};
    return result_of_card::SUCC;
}

constexpr bool is_trailer(uint8_t block) {
    return block < 128 ? (block % 4 == 3) : (block % 16 == 15);
}

constexpr uint8_t sector_of(uint8_t block) {
    return block < 128 ? block / 4 : 32 + ((block - 128) / 16);
}

result_of_card CardReader::start_alteration_op(uint8_t block_src, std::array<uint8_t, 6> keybuff, key keyv, uint8_t block_dst, ALTERATION_OP oper, int32_t operand){
    if (!std::holds_alternative<std::monostate>(op)) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    if (block_src == 0 || block_dst == 0 || is_trailer(block_src) || is_trailer(block_dst) || sector_of(block_src) != sector_of(block_dst)) {return result_of_card::OP_NOT_POSSIBLE;}
    op = alteration_un{oper, ALTERATION_STATE::IDLE, block_src, block_dst, keybuff, operand, keyv};
    return result_of_card::SUCC;
}

result_of_card CardReader::start_write_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv, std::array<uint8_t, 16>write_buff, bool REQUIRED){
    if (!std::holds_alternative<std::monostate>(op)) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    if (block == 0) return result_of_card::OP_NOT_POSSIBLE;
    if (((block % 4) == 3 && block < 128) || (block >= 128 && (block) % 16 == 15)) {
        if (!REQUIRED) return result_of_card::OP_NOT_POSSIBLE;
        auto byte6 = write_buff[6];
        auto byte7 = write_buff[7];
        auto byte8 = write_buff[8];
        bool correct = ((~(byte6 & 0x0F) & 0x0F) == (byte7 >> 4U)) && ((~(byte6 & 0xF0U) & 0xF0U) == ((byte8 & 0x0F) << 4U)) && ((~(byte7 & 0x0F) & 0x0F) == (byte8 >> 4U));
        if (!correct) return result_of_card::OP_NOT_POSSIBLE;
    }
    op = writing_un{WRITING_STATES::IDLE, block, keybuff, keyv, write_buff};
    return result_of_card::SUCC;
}

result_of_card CardReader::poll(){
    if (std::holds_alternative<std::monostate>(op)) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    result_of_card res;
    while (true) {
        if (std::holds_alternative<uid_un>(op)) {
            res = get_uid();
        }
        else if (std::holds_alternative<reading_un>(op)) {
            res = get_read();
        }
        else if (std::holds_alternative<writing_un>(op)) {
            res = get_write();
        }
        else if (std::holds_alternative<alteration_un>(op)) {
            res = get_alteration();
        } 
        else{
            return result_of_card::OP_NOT_POSSIBLE;
        }
        if (res == result_of_card::WAIT && !ic_com.get_isr_flag()) continue;
        if (res != result_of_card::WAIT) op = std::monostate();
        return res;
    }
}

result_of_card CardReader::step_preparing_card(uint8_t *keybuff, key keyv, uint8_t block){
    switch (prep_st) {
        case PREPARE_CARD_FOR_RW::SELECTING:
        {
            auto res = step_activating_card(true);
            if(res == result_of_card::WAIT) break;
            else if(res != result_of_card::SUCC) return res;
            prep_st = PREPARE_CARD_FOR_RW::AUTH_SENT;
            break;
        }
        case PREPARE_CARD_FOR_RW::AUTH_SENT:
        {
            if ((buff_uid_sak.second == 0x08 && block > 63) || (buff_uid_sak.second == 0x18 && block > 255) || (buff_uid_sak.second == 0x09 && block > 19) || (buff_uid_sak.second != 0x08 && buff_uid_sak.second != 0x18 && buff_uid_sak.second != 0x09)) {
                return result_of_card::OP_NOT_POSSIBLE;
            }
            uint8_t arr_for_auth[12];
            uint8_t size{};
            if (keyv == key::KeyA) arr_for_auth[size++] = CMD_AUTH_CODE[0];
            else arr_for_auth[size++] = CMD_AUTH_CODE[1];
            arr_for_auth[size++] = block;
            for(uint8_t i{0}; i < 6; i++){arr_for_auth[size++] = keybuff[i];}
            uint8_t offset{};
            if (buff_uid_sak.first.size == 10) offset = 6;
            else if (buff_uid_sak.first.size == 7) offset = 3;
            for (uint8_t i{}; i < 4; i++) {
                arr_for_auth[size++] = buff_uid_sak.first.bytes[i+offset];
            }
            ic_com.start_exc(arr_for_auth, 12, false, false, TIMEOUT_LEVELS::Ti5, way_of_send::MFAUNT);
            prep_st = PREPARE_CARD_FOR_RW::AUTH_WAIT;
            break;
        }
        case PREPARE_CARD_FOR_RW::AUTH_WAIT:
        {
            auto res_err = ic_com.check_auth();
            if (res_err == result_of_transaction::SUCC) return result_of_card::SUCC;
            else if (res_err == result_of_transaction::WAIT) break;
            else return result_of_card::AUTH_FAILED;
        }
    }
    return result_of_card::WAIT;
}

result_of_card CardReader::check_ack(){
    auto res = ic_com.check_exc();
    if (res == result_of_transaction::WAIT) return result_of_card::WAIT;
    if (res != result_of_transaction::SUCC){
        return convert_error(res);
    }
    uint8_t val{};
    res = ic_com.recieve_exc(&val, 1);
    if(res != result_of_transaction::SUCC){
        return convert_error(res);
    }
    val &= 0b0000'1111U;
    if (val == 0x0A) {
        return result_of_card::SUCC;
    }
    else if (val == 0x00 || val == 0x04) {
        return result_of_card::OP_NOT_POSSIBLE;
    }
    else{
        return result_of_card::BITERROR;
    }
}


result_of_card CardReader::get_read(){
    auto &st = std::get<reading_un>(op);
    switch (st.rstate_) {
        case READING_STATES::IDLE:
        {
            begin_op();
            st.rstate_ = READING_STATES::PREP_CARD_FOR_RW;
            break;
        }
        case READING_STATES::PREP_CARD_FOR_RW:
        {
            auto res_err = step_preparing_card(st.key_buff.data(), st.keyv, st.block);
            if (res_err == result_of_card::WAIT) break;
            if (res_err == result_of_card::SUCC){
                st.rstate_ = READING_STATES::READ_BOCK;
                ic_com.start_exc(std::array<uint8_t, 2>{CMD_MIFARE_READ, st.block}.data(), 2, false, true, TIMEOUT_LEVELS::Ti5);
                break;
            } 
            else {
                return res_err;
            }
        }
        case READING_STATES::READ_BOCK:
        {
            auto res = ic_com.check_exc();
            if (res == result_of_transaction::WAIT) break;
            if (res != result_of_transaction::SUCC){
                return convert_error(res);
            }
            res = ic_com.recieve_exc(buff_read.bytes, 16);
            if (res != result_of_transaction::SUCC){
                return result_of_card::OP_NOT_POSSIBLE;
            }
            buff_read.size = 16;
            st.rstate_ = READING_STATES::HALT;
            break;
        }
        case READING_STATES::HALT:
        {
            return halt();
        }
    }
    return result_of_card::WAIT;
}


result_of_card CardReader::get_write(){
    auto &st = std::get<writing_un>(op);
    switch (st.rstate_) {
        case WRITING_STATES::IDLE:
        {
            begin_op();   
            st.rstate_ = WRITING_STATES::PREP_CARD_FOR_RW;
            break;
        }
        case WRITING_STATES::PREP_CARD_FOR_RW:
        {
            auto res_err = step_preparing_card(st.key_buff.data(), st.keyv, st.block);
            if (res_err == result_of_card::WAIT) break;
            if (res_err == result_of_card::SUCC){
                st.rstate_ = WRITING_STATES::WRITING_PT1;
                ic_com.start_exc(std::array<uint8_t, 2>{CMD_MIFARE_WRITE, st.block}.data(), 2, false, true, TIMEOUT_LEVELS::Ti10, way_of_send::TRANSIEVE, false);
                break;
            } 
            else {
                return res_err;
            }
        }
        case WRITING_STATES::WRITING_PT1:
        {
            auto res = check_ack();
            if (res == result_of_card::WAIT) {break;}
            else if(res != result_of_card::SUCC) {
                return res;
            }
            ic_com.start_exc(st.write_buff.data(), 16, false, true, TIMEOUT_LEVELS::Ti10, way_of_send::TRANSIEVE, false);
            st.rstate_ = WRITING_STATES::WRITING_PT2;
            break;
        }
        case WRITING_STATES::WRITING_PT2:
        {
            auto res = check_ack();
            if (res == result_of_card::WAIT) {break;}
            else if (res != result_of_card::SUCC) {
                return res;
            }
            st.rstate_ = WRITING_STATES::HALT;
            break;
        }
        case WRITING_STATES::HALT:
        {
            return halt();
        }
    }
    return result_of_card::WAIT;
}

result_of_card CardReader::get_alteration(){
    auto &st = std::get<alteration_un>(op);
    switch (st.rstate_) {
        case ALTERATION_STATE::IDLE:
        {   
            begin_op();
            st.rstate_ = ALTERATION_STATE::PREP_CARD_FOR_RW;
            break;
        }
        case ALTERATION_STATE::PREP_CARD_FOR_RW:
        {
            auto res_err = step_preparing_card(st.key_buff.data(), st.keyv, st.block_src);
            if (res_err == result_of_card::WAIT) break;
            if (res_err == result_of_card::SUCC){
                st.rstate_ = ALTERATION_STATE::WRITING_PT1;
                ic_com.start_exc(std::array<uint8_t, 2>{static_cast<uint8_t>(st.op), st.block_src}.data(), 2, false, true, TIMEOUT_LEVELS::Ti5, way_of_send::TRANSIEVE, false);
                break;
            } 
            else {
                return res_err;
            }
        }
        case ALTERATION_STATE::WRITING_PT1:
        {
            auto res = check_ack();
            if (res == result_of_card::WAIT) {break;}
            else if (res != result_of_card::SUCC) {
                return res;
            }
            st.rstate_ = ALTERATION_STATE::WRITING_PT2;
            auto v = static_cast<uint32_t>(st.operand);
            std::array<uint8_t, 4> bytes{};
            for (uint8_t i{}; i < 4; i++) {
                bytes[i] = static_cast<uint8_t>(v >> (8U * i));
            }
            ic_com.start_exc(bytes.data(), bytes.size(), false, true, TIMEOUT_LEVELS::Ti5, way_of_send::TRANSIEVE, false);
            break;
        }
        case ALTERATION_STATE::WRITING_PT2:
        {
            auto res = ic_com.check_exc();
            if (res == result_of_transaction::WAIT) break;
            if (res != result_of_transaction::Time_out) {
                return convert_error(res);
            }
            ic_com.start_exc(std::array<uint8_t, 2>{CMD_MIFARE_TRANSFER, st.block_dst}.data(), 2, false, true, TIMEOUT_LEVELS::Ti10, way_of_send::TRANSIEVE, false);
            st.rstate_ = ALTERATION_STATE::TRANSFER;
            break;
        }
        case ALTERATION_STATE::TRANSFER:
        {
            auto res = check_ack();
            if (res == result_of_card::WAIT) {break;}
            else if (res != result_of_card::SUCC) {
                return res;
            }
            st.rstate_ = ALTERATION_STATE::HALT;
            break;
        }
        case ALTERATION_STATE::HALT:
        {
            return halt();
        }
    }
    return result_of_card::WAIT;
}


Uid CardReader::uid()const{return buff_uid_sak.first;}

uint8_t CardReader::Sak()const{return buff_uid_sak.second;}

Read_Block CardReader::block()const{return buff_read;}


 }// namespace rc522