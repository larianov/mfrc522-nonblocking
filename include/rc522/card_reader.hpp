#pragma once
#include "rc522/enums.hpp"
#include "rc522/rc522.hpp"
#include <array>
#include <cstdint>
#include <utility>
#include <variant>
namespace rc522 {

struct anti_col {
    uint8_t layer_{};
    Selecting state_{};
};

class CardReader {
  private:
    anti_col select_{};
    std::variant<std::monostate, uid_un, reading_un, writing_un, alteration_un> op;
    Halt_states halt_st_{};
    static result_of_card convert_error(result_of_transaction err);
    Rc522 &ic_com;
    result_of_card step_select_fsm();
    result_of_card halt();
    CARD_activate state_of_activating_card{};
    std::pair<Uid, uint8_t> buff_uid_sak;
    bool uid_unfull{};
    result_of_card step_activating_card(bool wupa = false);
    result_of_card check_ack();
    PREPARE_CARD_FOR_RW prep_st{};
    result_of_card step_preparing_card(uint8_t *keybuff, key keyv, uint8_t block);

    void begin_op();    
    result_of_card read_card_st{};
    Read_Block buff_read{};
    
    result_of_card get_uid();
    result_of_card get_read();
    result_of_card get_write();
    result_of_card get_alteration();
    
  public:
    
    [[nodiscard]]Uid uid() const;
    [[nodiscard]]uint8_t Sak() const; 
    [[nodiscard]]Read_Block block() const;
    
    result_of_card start_uid_transaction(WAKING_CARD_UP_FOR_UID wc = WAKING_CARD_UP_FOR_UID::REQA);
    result_of_card start_read_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv);
    result_of_card start_write_transaction(uint8_t block, std::array<uint8_t, 6> keybuff, key keyv, std::array<uint8_t, 16> write_buff, bool REQUIRED);
    result_of_card start_alteration_op(uint8_t block_src, std::array<uint8_t, 6> keybuff, key keyv, uint8_t block_dst, ALTERATION_OP oper, int32_t operand);
    
    result_of_card poll();

    void abort();

    explicit CardReader(Rc522 &ic_ref) : ic_com(ic_ref) {};
};
} // namespace rc522