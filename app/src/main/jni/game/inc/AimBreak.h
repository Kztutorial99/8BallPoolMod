#pragma once

// ── Aim Break ─────────────────────────────────────────────────────────────────
// 8-Ball Pool Break Shot:
//  - Scan ALL angles to find the one that pockets the MOST balls in one shot
//  - Minimum target: 2+ balls pocketed
//  - Cue ball must NOT be scratched
//  - 8-ball (index 8) should NOT be pocketed on break (illegal in most rules)
//  - Returns best break angle; if nothing pockets 2+, return best single-ball
// ─────────────────────────────────────────────────────────────────────────────

namespace AimBreak {
    bool bActive = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    void Aim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        double bestAngle     = startAngle;
        int    bestCount     = -1;
        bool   found2Plus    = false;

        double scanAngle = startAngle;
        for (int iter = 0; iter < (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 1; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];

            // No scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
                continue;
            }

            // Count pocketed balls (exclude cue ball and 8-ball for break)
            int potted = 0;
            bool eightPotted = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    if (i == 8) {
                        eightPotted = true;
                    } else {
                        potted++;
                    }
                }
            }

            // Skip if 8-ball is pocketed on break (foul in standard rules)
            if (eightPotted) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
                continue;
            }

            if (potted > bestCount) {
                bestCount  = potted;
                bestAngle  = scanAngle;
                if (potted >= 2) found2Plus = true;

                // If we found an angle that pockets 4+ balls, take it immediately
                if (potted >= 4) break;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
        }

        if (bestCount >= 1) {
            vg.mAimAngle(bestAngle);
        }
    }
}
