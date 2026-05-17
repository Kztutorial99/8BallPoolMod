#pragma once

// ── Aim Lock Target ───────────────────────────────────────────────────────────
// 8-Ball Pool: Lock aim on player's own balls (solid or stripe).
// - Detects player class automatically (SOLID / STRIPE)
// - Targets only player's own balls, skips opponent balls
// - Cue ball must hit player's ball FIRST
// - Does NOT pocket the 8-ball prematurely
// - Does NOT scratch (pocket cue ball)
// ─────────────────────────────────────────────────────────────────────────────

namespace AimLockTarget {
    bool bActive = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    // Score a candidate shot: higher is better
    // +10 per own ball potted, -100 if 8-ball potted early, -100 if scratch
    static double ScoreShot(Ball::Classification myclass, bool only8BLeft) {
        double score = 0.0;

        auto& cueBall   = gPrediction->guiData.balls[0];
        auto& eightBall = gPrediction->guiData.balls[8];

        // Scratch = bad
        if (!cueBall.onTable) return -1000.0;

        // 8-ball potted when not intended = bad
        if (!only8BLeft && !eightBall.onTable) return -1000.0;

        Ball::Classification target = only8BLeft
            ? Ball::Classification::EIGHT_BALL
            : myclass;

        // First hit must be our target type
        if (gPrediction->guiData.collision.firstHitBall) {
            if (gPrediction->guiData.collision.firstHitBall->classification != target)
                return -500.0;
        } else {
            return -500.0; // no contact at all
        }

        // Count own potted balls
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == target && ball.originalOnTable && !ball.onTable) {
                score += 10.0;
            }
            // Potting opponent's ball = bad
            if (!only8BLeft) {
                Ball::Classification opp = (myclass == Ball::Classification::SOLID)
                    ? Ball::Classification::STRIPE
                    : Ball::Classification::SOLID;
                if (ball.classification == opp && ball.originalOnTable && !ball.onTable) {
                    score -= 20.0;
                }
            }
        }

        return score;
    }

    void Aim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        // Can only work when player has a classification
        if (myclass == Ball::Classification::ANY ||
            myclass == Ball::Classification::ERR_CLASSIFICATION) {
            // Fallback to generic aim
            return;
        }

        bool only8BLeft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == myclass && ball.originalOnTable) {
                only8BLeft = false;
                break;
            }
        }

        double bestAngle = startAngle;
        double bestScore = -9999.0;

        double scanAngle = startAngle;
        for (int iter = 0; iter < (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 1; iter++) {
            gPrediction->determineShotResult(true, scanAngle);
            double score = ScoreShot(myclass, only8BLeft);
            if (score > bestScore) {
                bestScore = score;
                bestAngle = scanAngle;
            }
            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5 && iter > 0) break;
        }

        if (bestScore > 0.0) {
            vg.mAimAngle(bestAngle);
        }
    }
}
