#pragma once

// ── Enemy Sabotage — Runtime Memory Manipulation During Opponent's Turn ───────
// Modifies game state variables directly in memory while state == 7 (enemy turn)
//
// 3 Modes:
//   AIM_DEFLECT  — Adds a fixed angle offset to enemy mAimAngle every frame.
//                  Enemy sees correct guideline but shot goes to wrong direction.
//   POWER_DRAIN  — Forces mPower to near-zero so shot is too weak to reach pocket.
//   SPIN_CHAOS   — Injects extreme back+side english into mVisualEnglishControl.
//                  Enemy cue ball flies wildly after contact.
//
// State reference (GameStateManager):
//   4 = my turn    7 = opponent turn    8 = opponent can't shoot
// ──────────────────────────────────────────────────────────────────────────────

namespace EnemySabotage {

    bool  bActive  = false;
    int   iMode    = 0;     // 0=AimDeflect 1=PowerDrain 2=SpinChaos 3=Combo
    float fOffset  = 0.09f; // aim deflect in radians (~5.2 degrees)

    static bool IsEnemyTurn() {
        if (!sharedGameManager) return false;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return false;
        int sid = gsm.getCurrentStateId();
        return sid == 7 || sid == 8;
    }

    static void DoAimDeflect() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + (double)fOffset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    static void DoPowerDrain() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(0.015);
    }

    static void DoSpinChaos() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.95, -0.95));
    }

    void Update() {
        if (!bActive || !sharedGameManager) return;
        if (!IsEnemyTurn()) return;

        switch (iMode) {
            case 0: DoAimDeflect();                            break;
            case 1: DoPowerDrain();                            break;
            case 2: DoSpinChaos();                             break;
            case 3: DoAimDeflect(); DoPowerDrain(); DoSpinChaos(); break;
            default: break;
        }
    }

} // namespace EnemySabotage
