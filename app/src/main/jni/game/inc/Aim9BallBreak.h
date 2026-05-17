#pragma once

// ── Aim Break One Shot Win — 9 Ball Pool ─────────────────────────────────────
// Peraturan 9 Ball Pool Miniclip (wajib dipatuhi):
//  1. Cue ball HARUS mengenai bola NOMOR TERKECIL di meja PERTAMA (biasanya bola 1)
//  2. Setelah mengenai bola terendah, bola lain boleh masuk
//  3. Jika bola 9 masuk secara legal → MENANG INSTAN
//  4. Tidak boleh scratch (cue ball masuk pocket)
//
// Strategi Break:
//  - Scan semua sudut, validasi firstHitBall == bola terendah
//  - Prioritas 1: sudut yang memasukkan bola 9 (win instan!)
//  - Prioritas 2: sudut yang memasukkan bola paling banyak
//  - Jika tidak ada yang valid → cari sudut yang setidaknya kena bola terendah
// ─────────────────────────────────────────────────────────────────────────────

namespace Aim9BallBreak {
    bool bActive        = false;
    int  lastBestCount  = 0;
    int  lastTargetBall = -1;
    bool lastWasWin9    = false;

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS * 0.5;

    // Cari indeks bola nomor terkecil yang masih di meja
    static int FindLowestBall() {
        if (!gPrediction) return -1;
        for (int i = 1; i <= 9; i++) {
            if (i >= gPrediction->guiData.ballsCount) break;
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable) return i;
        }
        return -1;
    }

    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        int lowestBall = FindLowestBall();
        if (lowestBall < 0) return;   // tidak ada bola di meja

        // ── Prioritas 1: win 9 on break
        double win9Angle    = startAngle;
        bool   foundWin9    = false;

        // ── Prioritas 2: paling banyak bola masuk (first hit = lowest)
        double bestAngle    = startAngle;
        int    bestCount    = -1;
        int    bestFirstBall = -1;

        // ── Fallback: setidaknya kena bola terendah (tidak ada yang masuk)
        double fallbackAngle = startAngle;
        bool   foundFallback = false;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / ANGLE_STEP) + 4;

        for (int iter = 0; iter < maxIter; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall = gPrediction->guiData.balls[0];

            // Tidak boleh scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // ── Validasi wajib: first hit HARUS bola terkecil ──────────────
            if (!gPrediction->guiData.collision.firstHitBall) {
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            int firstHitIdx = gPrediction->guiData.collision.firstHitBall->index;
            if (firstHitIdx != lowestBall) {
                // First hit bukan bola terkecil → tidak valid sesuai peraturan
                scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
                continue;
            }

            // ── Hitung bola yang masuk ─────────────────────────────────────
            int  potted  = 0;
            bool nine_in = false;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    potted++;
                    if (i == 9) nine_in = true;
                }
            }

            // Fallback: setidaknya kena bola terendah (walau tidak ada yang masuk)
            if (!foundFallback) {
                fallbackAngle = scanAngle;
                foundFallback = true;
            }

            // Prioritas 1: win 9 instan (bola 9 masuk)
            if (nine_in && !foundWin9) {
                win9Angle = scanAngle;
                foundWin9 = true;
            }

            // Prioritas 2: paling banyak bola masuk
            if (potted > bestCount) {
                bestCount     = potted;
                bestAngle     = scanAngle;
                bestFirstBall = firstHitIdx;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < ANGLE_STEP * 0.5) break;
        }

        // ── Terapkan hasil ────────────────────────────────────────────────
        lastWasWin9    = foundWin9;
        lastBestCount  = (bestCount > 0) ? bestCount : 0;
        lastTargetBall = foundWin9 ? lowestBall : (bestCount >= 1 ? bestFirstBall : lowestBall);

        if (foundWin9) {
            vg.mAimAngle(win9Angle);
        } else if (bestCount >= 1) {
            vg.mAimAngle(bestAngle);
        } else if (foundFallback) {
            // Tidak ada yang masuk, tapi setidaknya kena bola terendah
            vg.mAimAngle(fallbackAngle);
        }
    }
}
