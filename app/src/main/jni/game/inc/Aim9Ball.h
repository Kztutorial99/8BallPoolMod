#pragma once

// ── Aim Predict WIN COMBO — 9 Ball (Ultra Fast, Win-Only) ─────────────────────
// 9-Ball Pool Rules:
//  - MUST hit the lowest numbered ball on table first
//  - Pocketing ball 9 on any legal shot = instant WIN
//
// Strategy (strict priority, ultra-fast scan):
//  1. WIN9  — ball 9 potted on this shot → instant win (100%). Break immediately.
//  2. COMBO — 2+ balls potted legally   → high-win combo. Keep scanning for best.
//  3. NO FALLBACK — single-ball shots are skipped entirely.
//
// Speed: scan step = MIN_ANGLE_STEP_RADIANS × 3 (3× faster than default).
//        Early exit the moment WIN9 is found.
// ─────────────────────────────────────────────────────────────────────────────

namespace Aim9Ball {
    bool bActive = false;

    int  lastTargetBall   = -1;
    int  lastTargetPocket = -1;
    bool lastWasCombo     = false;

    // 3× faster scan — still accurate enough for pot detection
    static constexpr double FAST_STEP = MIN_ANGLE_STEP_RADIANS * 3.0;

    static int FindLowestBall() {
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable) return i;
        }
        return -1;
    }

    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        gPrediction->determineShotResult(false,
            sharedGameManager.mVisualCue().getShotAngle());

        int lowestIdx = FindLowestBall();
        if (lowestIdx < 0) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        // Only 2 tiers — WIN9 and COMBO2+. No single-ball fallback.
        double win9Angle   = startAngle; bool foundWin9   = false;
        double combo2Angle = startAngle; bool foundCombo2 = false;
        int    winPocket   = -1;
        int    comboBall   = -1;
        int    comboPocket = -1;
        int    maxCombo    = 0;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / FAST_STEP) + 2;

        for (int iter = 0; iter < maxIter; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall = gPrediction->guiData.balls[0];

            // No scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + FAST_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < FAST_STEP * 0.5) break;
                continue;
            }

            // Must hit lowest ball first
            if (!gPrediction->guiData.collision.firstHitBall ||
                gPrediction->guiData.collision.firstHitBall->index != lowestIdx) {
                scanAngle = fmod(scanAngle + FAST_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < FAST_STEP * 0.5) break;
                continue;
            }

            // Count potted balls — only care about 2+ or ball-9
            int  potted    = 0;
            bool nine_in   = false;
            int  firstBall = -1;
            int  firstPock = -1;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    potted++;
                    if (firstBall < 0) { firstBall = i; firstPock = ball.pocketIndex; }
                    if (i == 9) nine_in = true;
                }
            }

            // ── Priority 1: WIN9 — ball 9 potted = instant win ────────────────
            if (nine_in && !foundWin9) {
                win9Angle  = scanAngle;
                foundWin9  = true;
                winPocket  = (9 < gPrediction->guiData.ballsCount)
                    ? gPrediction->guiData.balls[9].pocketIndex : -1;
                // WIN9 found — stop scanning immediately (max speed)
                break;
            }

            // ── Priority 2: COMBO 2+ — skip single-ball shots entirely ────────
            if (potted >= 2 && potted > maxCombo) {
                maxCombo    = potted;
                combo2Angle = scanAngle;
                foundCombo2 = true;
                comboBall   = firstBall;
                comboPocket = firstPock;
            }

            scanAngle = fmod(scanAngle + FAST_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < FAST_STEP * 0.5) break;
        }

        // ── Pick best candidate — no single-ball fallback ─────────────────────
        double finalAngle;
        if (foundWin9) {
            finalAngle       = win9Angle;
            lastTargetBall   = 9;
            lastTargetPocket = winPocket;
            lastWasCombo     = true;
        } else if (foundCombo2) {
            finalAngle       = combo2Angle;
            lastTargetBall   = comboBall;
            lastTargetPocket = comboPocket;
            lastWasCombo     = true;
        } else {
            // No winning shot found — do not aim at anything
            lastTargetBall   = -1;
            lastTargetPocket = -1;
            lastWasCombo     = false;
            return;
        }

        vg.mAimAngle(finalAngle);
    }
}
