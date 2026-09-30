#pragma once
#include "rc522/enums.hpp"
#include "rc522/rc522.hpp"
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
    std::variant<std::monostate, Uid_states, READING_STATES, WRITING_STATES> op;
    Halt_states halt_st_{};
    result_of_card convert_error(result_of_transaction err);
    Rc522 &ic_com;
    result_of_card step_select_fsm();
    result_of_card halt();
    Uid_activate state_of_activating_uid_{};
    std::pair<Uid, uint8_t> buff_uid_sak;
    bool uid_unfull{};
    result_of_card step_activating_card();
  public:
    explicit CardReader(Rc522 &ic_ref) : ic_com(ic_ref) {};
    std::pair<Uid, result_of_card> get_uid(); // pollable func
};
} // namespace rc522