#pragma once

// AutoPlay.h — State machine variables only.
// The Update() implementation is defined inline in menu.h (after all
// game objects and mod globals are in scope).

namespace AutoPlay {

    inline bool bAutoPlaying = false;

    enum class State : int {
        IDLE,
        SCANNING,   // AutoAim running — shows CALCULATING overlay
        SHOOTING,   // PowerSlider drag active
        COOLDOWN,   // Waiting for balls to settle after shot
    };

    inline State currentState = State::IDLE;
    inline float stateTimer   = 0.f;

    inline constexpr float SCAN_DURATION = 0.8f;   // seconds to aim before shooting
    inline constexpr float COOLDOWN_DUR  = 2.5f;   // seconds to wait after shot
    inline constexpr float SHOT_POWER    = 666.0f; // max power
    inline constexpr float DRAG_TIME     = 0.65f;  // seconds to drag power bar
    inline constexpr float HOLD_TIME     = 0.35f;  // hold at end before release

    inline void ClearState() {
        currentState = State::IDLE;
        stateTimer   = 0.f;
    }

    // Update() is defined later in menu.h — forward-declared here so callers compile.
    static void Update();

} // namespace AutoPlay
