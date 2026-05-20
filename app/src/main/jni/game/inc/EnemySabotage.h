#pragma once

// ── Enemy Sabotage — Runtime Memory Manipulation During Opponent's Turn ───────
// Modifies game state variables directly in memory while state == 7/8 (enemy turn)
//
// 14 Modes:
//   0  Aim Deflect    — fixed angle offset on enemy aim angle
//   1  Power Drain    — forces mPower to near-zero (too weak to reach pocket)
//   2  Spin Chaos     — injects extreme back+side english (wildly erratic)
//   3  Combo          — Deflect + Drain + Spin simultaneously
//   4  Aim Invert     — deflects aim by π rad (shoots exact opposite direction)
//   5  Micro Jitter   — rapid oscillating small deflect (hard to detect/correct)
//   6  Top Lock       — pure max topspin (cue ball rockets uncontrollably forward)
//   7  Power Surge    — forces mPower to 1.0 (overshoot → scratch / miss)
//   8  Chaos Extreme  — Invert + Surge + extreme spin all at once
//   9  Angle Zero     — lock aim angle to 0.0 (shoots straight into rail)
//  10  English Random — inject pseudo-random english XY every frame
//  11  Power Glitch   — oscillate power 0↔1 every tick (engine can't lock)
//  12  Angle Drift    — accumulating offset per frame (gets worse over time)
//  13  Total Blackout — ALL sabotages at max simultaneously (ultimate risk)
// ─────────────────────────────────────────────────────────────────────────────

namespace EnemySabotage {

    bool  bActive = false;
    int   iMode   = 0;
    float fOffset = 0.09f;   // aim deflect offset in radians

    static bool IsEnemyTurn() {
        if (!sharedGameManager) return false;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return false;
        int sid = gsm.getCurrentStateId();
        return sid == 7 || sid == 8;
    }

    // ── Mode 0: Aim Deflect ───────────────────────────────────────────────────
    static void DoAimDeflect() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + (double)fOffset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    // ── Mode 1: Power Drain ───────────────────────────────────────────────────
    static void DoPowerDrain() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(0.012);
    }

    // ── Mode 2: Spin Chaos ────────────────────────────────────────────────────
    static void DoSpinChaos() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.95, -0.95));
    }

    // ── Mode 4: Aim Invert — exact opposite direction (π offset) ─────────────
    static void DoAimInvert() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double inverted = fmod(cur + 3.14159265358979323846 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(inverted));
    }

    // ── Mode 5: Micro Jitter — small oscillating deflect, hard to correct ─────
    static void DoMicroJitter() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        static int jTick = 0;
        jTick++;
        float jRad = sinf((float)jTick * 0.43f) * 0.036f
                   + cosf((float)jTick * 0.27f) * 0.022f;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + (double)jRad + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    // ── Mode 6: Top Lock — pure topspin max, cue rockets out of control ───────
    static void DoTopLock() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.0, 0.99));
    }

    // ── Mode 7: Power Surge — max power → overshoot / scratch ────────────────
    static void DoPowerSurge() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(1.0);
    }

    // ── Mode 8: Chaos Extreme — full multi-axis sabotage ─────────────────────
    static void DoChaosExtreme() {
        DoAimInvert();
        DoPowerSurge();
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.99, 0.99));
    }

    // ── Mode 9: Angle Zero — lock aim to 0.0 rad (shoots into rail) ──────────
    static void DoAngleZero() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(0.0));
    }

    // ── Mode 10: English Random — pseudo-random english every frame ───────────
    static void DoEnglishRandom() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        static unsigned int rSeed = 0x1337BEEF;
        rSeed ^= rSeed << 13;
        rSeed ^= rSeed >> 17;
        rSeed ^= rSeed << 5;
        float rx = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;  // -1..1
        rSeed ^= rSeed << 13;
        rSeed ^= rSeed >> 17;
        float ry = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;  // -1..1
        vec.mEnglish(Vec2d((double)rx, (double)ry));
    }

    // ── Mode 11: Power Glitch — oscillate power 0↔1 every tick ──────────────
    static void DoPowerGlitch() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        static bool glitchFlip = false;
        glitchFlip = !glitchFlip;
        vc.mPower(glitchFlip ? 1.0 : 0.005);
    }

    // ── Mode 12: Angle Drift — accumulating aim offset (worsens over time) ───
    static void DoAngleDrift() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        static double driftAccum = 0.0;
        driftAccum += 0.008;   // ~0.46 deg per frame — builds fast
        if (driftAccum > MAX_ANGLE_RADIANS) driftAccum -= MAX_ANGLE_RADIANS;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double drifted = fmod(cur + driftAccum + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(drifted));
    }

    // ── Mode 13: Total Blackout — every sabotage at max simultaneously ────────
    static void DoTotalBlackout() {
        DoAngleZero();
        DoPowerSurge();
        DoSpinChaos();
        DoEnglishRandom();
        // Also drain power on alternating frames for extra glitch
        static bool bbFlip = false;
        bbFlip = !bbFlip;
        if (bbFlip) DoPowerDrain();
    }

    // ─────────────────────────────────────────────────────────────────────────
    void Update() {
        if (!bActive || !sharedGameManager) return;
        if (!IsEnemyTurn()) return;

        switch (iMode) {
            case 0:  DoAimDeflect();                                    break;
            case 1:  DoPowerDrain();                                    break;
            case 2:  DoSpinChaos();                                     break;
            case 3:  DoAimDeflect(); DoPowerDrain(); DoSpinChaos();     break;
            case 4:  DoAimInvert();                                     break;
            case 5:  DoMicroJitter();                                   break;
            case 6:  DoTopLock();                                       break;
            case 7:  DoPowerSurge();                                    break;
            case 8:  DoChaosExtreme();                                  break;
            case 9:  DoAngleZero();                                     break;
            case 10: DoEnglishRandom();                                 break;
            case 11: DoPowerGlitch();                                   break;
            case 12: DoAngleDrift();                                    break;
            case 13: DoTotalBlackout();                                 break;
            default: break;
        }
    }

} // namespace EnemySabotage
