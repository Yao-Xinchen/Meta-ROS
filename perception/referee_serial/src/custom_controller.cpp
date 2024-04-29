#include "referee_serial/custom_controller.hpp"

bool CustomController::is_wanted_pre(const std::vector<uint8_t> &prefix)
{
    if (prefix[0] != 0xA5) return false;

    uint16_t length = static_cast<uint16_t>(prefix[1]) | (static_cast<uint16_t>(prefix[2]) << 8);
    uint16_t cmd_id = static_cast<uint16_t>(prefix[5]) | (static_cast<uint16_t>(prefix[6]) << 8);

    if (cmd_id != 0x0302 || length != 30) return false;
    return true;
}

CustomController::CustomController(const std::vector<uint8_t> &frame)
{
    // copy the uint8_t vector to the struct
    std::copy(frame.begin(), frame.end(), reinterpret_cast<uint8_t *>(&interpreted));
}

operation_interface::msg::CustomController CustomController::msg()
{
    operation_interface::msg::CustomController msg;
    // to be defined
    return msg;
}