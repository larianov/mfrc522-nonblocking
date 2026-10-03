#pragma once
#include "rc522/enums.hpp"

static const char *result_name(rc522::result_of_card res) {
    switch (res) {
    case rc522::result_of_card::WAIT: return "WAIT";
    case rc522::result_of_card::SUCC: return "SUCC";
    case rc522::result_of_card::NO_CARD: return "NO_CARD";
    case rc522::result_of_card::COLLISION: return "COLLISION";
    case rc522::result_of_card::BITERROR: return "BITERROR";
    case rc522::result_of_card::TIMEOUT: return "TIMEOUT";
    case rc522::result_of_card::OP_NOT_POSSIBLE: return "OP_NOT_POSSIBLE";
    case rc522::result_of_card::KEY_WAS_REJECTED: return "KEY_WAS_REJECTED";
    case rc522::result_of_card::ERROR_FROM_IC: return "ERROR_FROM_IC";
    case rc522::result_of_card::AUTH_FAILED: return "AUTH_FAILED";
    }
    return "UNKNOWN";
}
