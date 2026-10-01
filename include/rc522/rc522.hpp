#pragma once
#include "transport.hpp"
#include <cstdint>
#include "enums.hpp"

namespace rc522 {
class Rc522 {
  private:
    uint8_t garbage[64]{};
    uint8_t fifo_buff[65]{};
    uint8_t version_{0xFF};
    Transport &t_;
    TIMEOUT_LEVELS last_time_{TIMEOUT_LEVELS::Ti1};
    result_of_transaction error_decoding();
    void write_one_byte(uint8_t address, uint8_t value_to_write);
    void read_n_bytes(uint8_t *buff, uint8_t address, uint16_t N);
    void clear_status();
  public:
    explicit Rc522(Transport &t);
    result_of_op init();
    uint32_t get_time();
    uint8_t read_one_byte(uint8_t address); 
    result_of_op change_gain(RFCfgReg_Gain value);
    const char *version(uint8_t &version_mut) const;
    result_of_op set_power_state(uint8_t power_up); // 1 for power up, 0 for power_down
    void start_exc(const uint8_t *arr, uint16_t size, bool byt7e, bool crc, TIMEOUT_LEVELS time_levels, way_of_send wayt = way_of_send::TRANSIEVE);
    result_of_transaction check_exc();
    result_of_transaction check_auth();
    result_of_transaction recieve_exc(uint8_t *arr, uint8_t size);
};
} // namespace rc522