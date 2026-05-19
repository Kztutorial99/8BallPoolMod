#pragma once

// ── Aim Break (8 Ball) ────────────────────────────────────────────────────────
// Scan ALL angles to find the break that pockets the MOST balls.
// Rules:
//  - Cue ball must NOT be scratched
//  - 8-ball (index 8) should NOT be pocketed on break (foul in standard rules)
//  - Scan every angle, keep absolute best (max balls pocketed)
//  - Tiebreaker: BreakSpecial::ScoreAngleDifficulty() — pilih angle yang buat
//    posisi sisa bola musuh paling susah (dekat rail, jauh dari pocket)
// ─────────────────────────────────────────────────────────────────────────────

#include "BreakSpecial.h"

namespace AimBreak {
    bool bActive = false;

    // Last result info (used by animation overlay)
    int  lastBestCount  = 0;
    int  lastTargetBall = -1;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS * 0.5; // finer step for break

    void Aim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        double bestAngle      = startAngle;
        int    bestCount      = -1;
        double bestDiffScore  = -1.0;  // [A] tiebreaker: kesulitan posisi sisa bola
        int    bestFirstBall  = -1;

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

            // Count pocketed balls (exclude cue ball and 8-ball on break)
            int  potted       = 0;
            bool eightPotted  = false;
            int  firstBallIdx = -1;

            if (gPrediction->guiData.collision.firstHitBall) {
                firstBallIdx = gPrediction->guiData.collision.firstHitBall->index;
            }

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

            // Skip if 8-ball pocketed on break (foul)
            if (eightPotted) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // [A] Hitung skor kesulitan posisi sisa bola musuh
            double diffScore = BreakSpecial::bEnabled
                ? BreakSpecial::ScoreAngleDifficulty()
                : 0.0;

            // Pilih: lebih banyak bola masuk = prioritas utama
            //        sama banyak → pilih yang skor kesulitannya lebih tinggi
            bool better = false;
            if (potted > bestCount) {
                better = true;
            } else if (potted == bestCount && diffScore > bestDiffScore) {
                better = true;
            }

            if (better) {
                bestCount      = potted;
                bestDiffScore  = diffScore;
                bestAngle      = scanAngle;
                bestFirstBall  = firstBallIdx;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
        }

        lastBestCount  = (bestCount > 0) ? bestCount : 0;
        lastTargetBall = bestFirstBall;

        if (bestCount >= 1) {
            vg.mAimAngle(bestAngle);
        }
    }
}
