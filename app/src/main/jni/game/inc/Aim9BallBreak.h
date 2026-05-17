#pragma once

// ── Aim Break One Shot Win — 9 Ball ──────────────────────────────────────────
// 9-Ball break strategy:
//  - Scan all angles for the break shot
//  - Priority 1: pockets ball 9 on break (instant win!)
//  - Priority 2: pockets the most balls (good break position)
//  - Must always make sure cue ball does NOT scratch
//  - Try to hit ball 1 (or lowest ball) first — standard 9-ball break rule
// ─────────────────────────────────────────────────────────────────────────────

namespace Aim9BallBreak {
    bool bActive = false;

    int lastBestCount   = 0;
    int lastTargetBall  = -1;
    bool lastWasWin9    = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS * 0.5; // finer step for break

    void Aim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        double win9Angle   = startAngle; bool foundWin9 = false;
        double bestAngle   = startAngle; int  bestCount = -1;
        int    bestFirst   = -1;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 2;

        for (int iter = 0; iter < maxIter; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall = gPrediction->guiData.balls[0];

            // No scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            int  potted     = 0;
            bool nine_in    = false;
            int  firstBall  = -1;

            if (gPrediction->guiData.collision.firstHitBall) {
                firstBall = gPrediction->guiData.collision.firstHitBall->index;
            }

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    potted++;
                    if (i == 9) nine_in = true;
                }
            }

            // Priority 1: ball 9 pocketed on break = instant win
            if (nine_in && !foundWin9) {
                win9Angle  = scanAngle;
                foundWin9  = true;
                lastWasWin9 = true;
            }

            // Track best overall break (most balls)
            if (potted > bestCount) {
                bestCount  = potted;
                bestAngle  = scanAngle;
                bestFirst  = firstBall;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
        }

        lastBestCount  = (bestCount > 0) ? bestCount : 0;
        lastTargetBall = bestFirst;
        lastWasWin9    = foundWin9;

        // Apply best angle
        double finalAngle = foundWin9 ? win9Angle : bestAngle;
        if (bestCount >= 1) {
            vg.mAimAngle(finalAngle);
        }
    }
}
