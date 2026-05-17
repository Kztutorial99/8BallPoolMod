#pragma once

// ── Aim 9 Ball ────────────────────────────────────────────────────────────────
// 9-Ball Pool Rules:
//  - Must hit the LOWEST numbered ball on table first
//  - Ball 9 can be pocketed on ANY shot (including first break) = WIN
//  - Strategy: aim to hit lowest ball AND try to combo-pocket ball 9
// Logic:
//  1. Find the lowest numbered ball still on table
//  2. Scan all angles where cue ball hits that ball first
//  3. Among valid angles, prefer one that also pockets ball 9 (combo/one-shot)
//  4. If no combo found, just find cleanest hit on lowest ball
// ─────────────────────────────────────────────────────────────────────────────

namespace Aim9Ball {
    bool bActive = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    // Find the index of the lowest numbered ball still on table (balls 1-9)
    // Returns -1 if no balls found
    static int FindLowestBall() {
        for (int i = 1; i <= 9 && i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable) return i;
        }
        return -1;
    }

    void Aim() {
        if (!sharedGameManager) return;

        // Reinitialize prediction data from current game state
        gPrediction->determineShotResult(false, sharedGameManager.mVisualCue().getShotAngle());

        int lowestIdx = FindLowestBall();
        if (lowestIdx < 0) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        double bestAngle       = startAngle;
        double bestComboAngle  = -1.0; // angle that also pots ball 9
        bool   foundClean      = false;
        bool   foundCombo      = false;

        double scanAngle = startAngle;
        for (int iter = 0; iter < (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 1; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall = gPrediction->guiData.balls[0];

            // Cue ball must stay on table (no scratch)
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
                continue;
            }

            // First hit must be the lowest ball
            if (!gPrediction->guiData.collision.firstHitBall) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
                continue;
            }
            if (gPrediction->guiData.collision.firstHitBall->index != lowestIdx) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
                continue;
            }

            // Valid: hits lowest ball first
            if (!foundClean) {
                bestAngle  = scanAngle;
                foundClean = true;
            }

            // Check if ball 9 is also pocketed (combo / one-shot win)
            if (lowestIdx != 9 && 9 < gPrediction->guiData.ballsCount) {
                auto& ball9 = gPrediction->guiData.balls[9];
                if (ball9.originalOnTable && !ball9.onTable) {
                    // Ball 9 potted = instant win candidate
                    bestComboAngle = scanAngle;
                    foundCombo     = true;
                    break; // take the first combo found
                }
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
        }

        // Prefer combo (pots ball 9), fallback to clean hit on lowest
        double finalAngle = foundCombo ? bestComboAngle : (foundClean ? bestAngle : startAngle);
        vg.mAimAngle(finalAngle);
    }
}
