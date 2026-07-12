#pragma once
#include <cstdint>

namespace input {

enum class MouseButton {
    Left,
    Right,
    Middle
};

enum class Key {
    Unknown,
    Left, Right, Up, Down,
    Space, Return, Escape,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Delete, BackSpace, Tab,
    LShift, RShift, LControl, RControl, LAlt, RAlt
};

struct Event {
    enum Type {
        Closed,
        KeyPressed,
        KeyReleased,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseWheelScrolled,
        TextEntered,
        TouchBegan,
        TouchMoved,
        TouchEnded,
        PinchZoom,
    } type;

    struct KeyEvent {
        Key code;
        bool shift = false;
        bool control = false;
        bool alt = false;
    };

    struct MouseButtonEvent {
        MouseButton button;
        int x = 0;
        int y = 0;
    };

    struct MouseMoveEvent {
        int x = 0;
        int y = 0;
    };

    struct MouseWheelEvent {
        float delta = 0;
        int x = 0;
        int y = 0;
    };

    struct TextEvent {
        uint32_t unicode = 0;
    };

    struct TouchEvent {
        int finger = 0;
        int x = 0;
        int y = 0;
    };

    struct PinchEvent {
        float dDist = 0;
    };

    KeyEvent key;
    MouseButtonEvent mouseButton;
    MouseMoveEvent mouseMove;
    MouseWheelEvent mouseWheel;
    TextEvent text;
    TouchEvent touch;
    PinchEvent pinch;
};

} // namespace input
