/**
 * @file utils/input_injector.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "utils/input_injector.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace frqs::utils {

#ifdef _WIN32

bool InputInjector::moveMouse(double x, double y) {
    // Clamp to [0, 1]
    x = (x < 0.0) ? 0.0 : (x > 1.0) ? 1.0 : x;
    y = (y < 0.0) ? 0.0 : (y > 1.0) ? 1.0 : y;
    
    // Convert to absolute coordinates (0-65535)
    LONG abs_x = static_cast<LONG>(x * 65535.0);
    LONG abs_y = static_cast<LONG>(y * 65535.0);
    
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = abs_x;
    input.mi.dy = abs_y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool InputInjector::clickLeft() {
    return mouseDown(true) && mouseUp(true);
}

bool InputInjector::clickRight() {
    return mouseDown(false) && mouseUp(false);
}

bool InputInjector::mouseDown(bool left) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = left ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool InputInjector::mouseUp(bool left) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = left ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
    
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool InputInjector::pressKey(uint16_t virtual_key_code) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtual_key_code;
    input.ki.dwFlags = 0;
    
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool InputInjector::releaseKey(uint16_t virtual_key_code) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtual_key_code;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool InputInjector::typeKey(uint16_t virtual_key_code) {
    return pressKey(virtual_key_code) && releaseKey(virtual_key_code);
}

#else
// Stub for non-Windows platforms
bool InputInjector::moveMouse(double, double) { return false; }
bool InputInjector::clickLeft() { return false; }
bool InputInjector::clickRight() { return false; }
bool InputInjector::mouseDown(bool) { return false; }
bool InputInjector::mouseUp(bool) { return false; }
bool InputInjector::pressKey(uint16_t) { return false; }
bool InputInjector::releaseKey(uint16_t) { return false; }
bool InputInjector::typeKey(uint16_t) { return false; }
#endif

} // namespace frqs::utils