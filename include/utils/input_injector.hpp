#pragma once

/**
 * @file utils/input_injector.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <cstdint>

namespace frqs::utils {

class InputInjector {
public:
    InputInjector() = default;
    
    // Move mouse to normalized coordinates (0-65535)
    // x, y are in screen percentage (0.0 to 1.0)
    bool moveMouse(double x, double y);
    
    // Click mouse buttons
    bool clickLeft();
    bool clickRight();
    bool mouseDown(bool left = true);
    bool mouseUp(bool left = true);
    
    // Keyboard input
    bool pressKey(uint16_t virtual_key_code);
    bool releaseKey(uint16_t virtual_key_code);
    bool typeKey(uint16_t virtual_key_code);  // Press + release
    
    // Common virtual key codes (for reference)
    static constexpr uint16_t KEY_LEFT_BUTTON = 0x01;
    static constexpr uint16_t KEY_RIGHT_BUTTON = 0x02;
    static constexpr uint16_t KEY_RETURN = 0x0D;
    static constexpr uint16_t KEY_ESCAPE = 0x1B;
    static constexpr uint16_t KEY_SPACE = 0x20;
    
private:
    bool sendInput(uint32_t type, uint32_t flags, uint16_t data);
};

} // namespace frqs::utils