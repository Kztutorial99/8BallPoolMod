#pragma once

// ── Aim Predict + Combo — 9 Ball ──────────────────────────────────────────────
// 9-Ball Pool Rules:
//  - MUST hit the lowest numbered ball on table first
//  - Any ball can be pocketed after that (combinations are legal)
//  - Pocketing ball 9 on any legal shot = instant WIN
// Strategy:
//  1. Scan all angles where cue hits lowest ball first
//  2. Priority: angles that also pocket ball 9 (instant win)
//  3. Otherwise: angles that pocket 2+ balls (combo)
//  4. Fallback: best single ball, still hitting lowest first
// ─────────────────────────────────────────────────────────────────────────────

namespace Aim9Ball {
    bool bActive = false;

    int lastTargetBall   = -1;
    int lastTargetPocket = -1;
    bool lastWasCombo    = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    static int FindLowestBall() {
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable) return i;
        }
        return -1;
    }

    void Aim() {
        if (!sharedGameManager) return;

        // Init prediction with current game state
        gPrediction->determineShotResult(false,
            sharedGameManager.mVisualCue().getShotAngle());

        int lowestIdx = FindLowestBall();
        if (lowestIdx < 0) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        // Candidates by priority
        double win9Angle    = startAngle; bool foundWin9    = false; // ball 9 also potted
        double combo2Angle  = startAngle; bool foundCombo2  = false; // 2+ balls potted
        double cleanAngle   = startAngle; bool foundClean   = false; // hits lowest, 1 ball
        int    winPocket    = -1;
        int    comboBall    = -1;
        int    cleanBall    = -1;
        int    cleanPocket  = -1;
        int    comboPocket  = -1;
        int    maxCombo     = 0;

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

            // Must hit lowest ball first
            if (!gPrediction->guiData.collision.firstHitBall ||
                gPrediction->guiData.collision.firstHitBall->index != lowestIdx) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // Count potted balls
            int potted     = 0;
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

            // Priority 1: ball 9 potted = win
            if (nine_in && !foundWin9) {
                win9Angle  = scanAngle;
                foundWin9  = true;
                winPocket  = 9 < gPrediction->guiData.ballsCount
                    ? gPrediction->guiData.balls[9].pocketIndex : -1;
            }

            // Priority 2: 2+ balls potted (combo)
            if (potted >= 2 && potted > maxCombo) {
                maxCombo    = potted;
                combo2Angle = scanAngle;
                foundCombo2 = true;
                comboBall   = firstBall;
                comboPocket = firstPock;
            }

            // Priority 3: any clean valid hit
            if (!foundClean && potted >= 1) {
                cleanAngle  = scanAngle;
                foundClean  = true;
                cleanBall   = firstBall;
                cleanPocket = firstPock;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
        }

        // Pick best candidate
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
        } else if (foundClean) {
            finalAngle       = cleanAngle;
            lastTargetBall   = cleanBall;
            lastTargetPocket = cleanPocket;
            lastWasCombo     = false;
        } else {
            return; // no valid shot found
        }

        vg.mAimAngle(finalAngle);
    }
}
