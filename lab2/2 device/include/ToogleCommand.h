#pragma once
#include <cstdint>

enum class ToogleCommand : uint8_t {
    OFF = 0XA,  // Можливо, варто змінити на більш читабельний формат
    ON = 0X14,
    SUCCESSFULLY_RECEIVED = 0X28,
    STOP = 0X42  // Додаємо команду STOP з відповідним значенням
};