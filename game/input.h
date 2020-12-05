#pragma once

#include "base/vectypes.h"

// Button definitions.
enum {
    BUTTON_A = 0x8000,
    BUTTON_B = 0x4000,

    BUTTON_L = 0x0020,
    BUTTON_R = 0x0010,
    BUTTON_Z = 0x2000,

    BUTTON_START = 0x1000,

    // D-pad.
    BUTTON_D_UP = 0x0800,
    BUTTON_D_DOWN = 0x0400,
    BUTTON_D_LEFT = 0x0200,
    BUTTON_D_RIGHT = 0x0100,

    // C-pad.
    BUTTON_C_UP = 0x0008,
    BUTTON_C_DOWN = 0x0004,
    BUTTON_C_LEFT = 0x0002,
    BUTTON_C_RIGHT = 0x0001,
};

// Controller input.
struct controller_input {
    unsigned buttons;
    vec2 joystick;
};