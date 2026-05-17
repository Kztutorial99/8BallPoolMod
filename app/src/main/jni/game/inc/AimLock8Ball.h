#pragma once

// ── Aim Lock 8 Ball — 8 Ball Pool ─────────────────────────────────────────────
// Mode khusus: hanya menarget bola 8 saat semua bola sendiri sudah masuk.
// Aturan (8-ball Miniclip):
//  - Cue ball HARUS mengenai bola 8 PERTAMA (direct shot)
//  - Bola 8 harus masuk pocket
//  - Cue ball tidak boleh scratch
//  - Pilih pocket terbaik secara otomatis (skor tertinggi)
//  - Jika ada bola lawan yang ikut masuk → penalti skor (hindari)
// ─────────────────────────────────────────────────────────────────────────────

namespace AimLock8Ball {
    bool bActive          = false;
    int  lastTargetBall   = 8;
    int  lastTargetPocket = -1;
    bool lastHadShot      = false;

    static constexpr double COARSE_STEP = MIN_ANGLE_STEP_RADIANS;
    static constexpr double FINE_STEP   = MIN_ANGLE_STEP_RADIANS * 0.25;

    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        if (8 >= gPrediction->guiData.ballsCount) { lastHadShot = false; return; }

        // ── Phase 1: Coarse scan (full circle) ────────────────────────────────
        // Pocket pilihan user (-1 = auto)
        int forcePocket = PocketSelector::Get();

        double bestAngle  = startAngle;
        int    bestScore  = -9999;
        int    bestPocket = -1;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / COARSE_STEP) + 4;

        for (int iter = 0; iter < maxIter; iter++) {
            gPrediction->determineShotResult(true, scanAngle);

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];

            // Tidak boleh scratch
            if (!cueBall.onTable) {
                scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
                continue;
            }

            // First hit HARUS bola 8
            if (!gPrediction->guiData.collision.firstHitBall ||
                gPrediction->guiData.collision.firstHitBall->index != 8) {
                scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
                continue;
            }

            // Bola 8 harus masuk pocket
            if (eightBall.onTable || !eightBall.originalOnTable) {
                scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
                continue;
            }

            // Hitung skor — penalti jika bola lawan ikut masuk
            int score = 100;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (i == 8) continue;
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) score -= 40;
            }

            if (score > bestScore && (forcePocket < 0 || eightBall.pocketIndex == forcePocket)) {
                bestScore  = score;
                bestAngle  = scanAngle;
                bestPocket = eightBall.pocketIndex;
            }

            scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
        }

        if (bestScore < 0) {
            lastHadShot      = false;
            lastTargetPocket = -1;
            return;
        }

        // ── Phase 2: Fine refinement di sekitar best angle ─────────────────────
        double refineStart = bestAngle - COARSE_STEP;
        double refineEnd   = bestAngle + COARSE_STEP;
        double fineAngle   = refineStart;

        while (true) {
            fineAngle = NumberUtils::normalizeDoublePrecision(
                fmod(fineAngle + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS));

            gPrediction->determineShotResult(true, fineAngle);

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];

            if (cueBall.onTable &&
                gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->index == 8 &&
                !eightBall.onTable && eightBall.originalOnTable) {

                int score = 100;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (i == 8) continue;
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) score -= 40;
                }

                if (score > bestScore && (forcePocket < 0 || eightBall.pocketIndex == forcePocket)) {
                    bestScore  = score;
                    bestAngle  = fineAngle;
                    bestPocket = eightBall.pocketIndex;
                }
            }

            fineAngle += FINE_STEP;
            if (fineAngle - refineEnd > 0.0) break;
        }

        lastTargetBall   = 8;
        lastTargetPocket = bestPocket;
        lastHadShot      = true;
        vg.mAimAngle(bestAngle);

        // Nominasi pocket — pakai pocket pilihan user jika ada, else best pocket
        int pktToNominate8 = (forcePocket >= 0) ? forcePocket : bestPocket;
        if (pktToNominate8 >= 0 && sharedGameManager)
            sharedGameManager.nominatePocket(pktToNominate8);
    }
}
