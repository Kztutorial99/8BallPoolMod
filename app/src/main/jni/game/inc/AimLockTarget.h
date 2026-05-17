#pragma once

// ── Aim Predict — 8 Ball ──────────────────────────────────────────────────────
// 8-Ball Pool: Aim at player's own balls (solid/stripe).
// Rules strictly enforced:
//  - Cue ball must hit PLAYER'S OWN ball FIRST (not opponent's ball)
//  - Pocket own balls only (solid/stripe depending on player class)
//  - Do NOT pocket 8-ball unless all own balls are already potted
//  - Do NOT scratch (pocket cue ball)
//  - Do NOT pocket opponent balls (penalty)
//  - Score = max own balls potted, disqualify if violates rules
// ─────────────────────────────────────────────────────────────────────────────

namespace AimLockTarget {
    bool bActive = false;

    // Last result for animation overlay
    int lastTargetBall   = -1;
    int lastTargetPocket = -1;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    void Aim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        // Only works after ball assignment
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) {
            // Fallback: try ANY ball that's not 8-ball
            myclass = Ball::Classification::SOLID;
        }

        // Check if only 8-ball left
        bool only8BLeft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == myclass && ball.originalOnTable) {
                only8BLeft = false;
                break;
            }
        }

        Ball::Classification target = only8BLeft
            ? Ball::Classification::EIGHT_BALL
            : myclass;

        Ball::Classification opponent = (myclass == Ball::Classification::SOLID)
            ? Ball::Classification::STRIPE
            : Ball::Classification::SOLID;

        double bestAngle      = startAngle;
        int    bestScore      = -9999;
        int    bestBallIdx    = -1;
        int    bestPocketIdx  = -1;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 2;

        for (int iter = 0; iter < maxIter; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];

            // No scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // Cue ball must hit OWN target ball first (critical 8-ball rule)
            if (!gPrediction->guiData.collision.firstHitBall) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            auto* firstHit = gPrediction->guiData.collision.firstHitBall;
            if (firstHit->classification != target) {
                // First hit is wrong ball type — skip (foul)
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // 8-ball potted prematurely = foul
            if (!only8BLeft && !eightBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // Count own potted balls, penalize opponent pots
            int score         = 0;
            int ownPotted     = 0;
            int firstOwnBall  = -1;
            int firstPocket   = -1;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (!ball.originalOnTable) continue;
                if (ball.onTable) continue;

                if (ball.classification == target) {
                    score += 15;
                    ownPotted++;
                    if (firstOwnBall < 0) {
                        firstOwnBall = i;
                        firstPocket  = ball.pocketIndex;
                    }
                } else if (ball.classification == opponent) {
                    score -= 25; // potting opponent = bad
                }
            }

            if (ownPotted > 0 && score > bestScore) {
                bestScore     = score;
                bestAngle     = scanAngle;
                bestBallIdx   = firstOwnBall;
                bestPocketIdx = firstPocket;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
        }

        lastTargetBall   = bestBallIdx;
        lastTargetPocket = bestPocketIdx;

        if (bestScore > 0) {
            vg.mAimAngle(bestAngle);
        }
    }
}
