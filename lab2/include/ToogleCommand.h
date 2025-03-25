#pragma once
#include <cstdint>

enum class ToogleCommand : uint8_t {
    OFF = 0XA,
    ON = 0X14,
    SUCCESSFULLY_RECEIVED = 0X28
};
