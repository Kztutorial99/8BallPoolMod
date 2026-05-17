#pragma once

// ── Aim Predict — 8 Ball Pool ─────────────────────────────────────────────────
// Peraturan 8 Ball Pool Miniclip (wajib dipatuhi):
//  1. Cue ball HARUS mengenai bola milik sendiri PERTAMA (solid/stripe sesuai kelas)
//  2. Jika semua bola sendiri sudah masuk → target adalah 8-ball
//  3. Jangan pocket 8-ball sebelum semua bola sendiri masuk (foul)
//  4. Jangan scratch (cue ball masuk pocket)
//  5. Jangan pocket bola lawan (foul / giliran lawan)
//
// Strategi:
//  - Scan semua sudut, pilih yang paling banyak pocket bola sendiri
//  - Penalti berat jika bola lawan ikut masuk
//  - Refinement: setelah dapat best angle, scan step lebih kecil di sekitarnya
//  - Fallback: jika tidak ada pocket sama sekali, set status "no shot" (tidak ubah aim)
// ─────────────────────────────────────────────────────────────────────────────

namespace AimLockTarget {
    bool bActive         = false;
    int  lastTargetBall  = -1;
    int  lastTargetPocket = -1;
    bool lastHadShot     = false;   // false = tidak ada shot valid ditemukan

    static constexpr double COARSE_STEP = MIN_ANGLE_STEP_RADIANS;
    static constexpr double FINE_STEP   = MIN_ANGLE_STEP_RADIANS * 0.25;

    // Cari kelas player — return SOLID/STRIPE jika sudah di-assign, SOLID (0) sentinel jika belum
    static Ball::Classification DetectMyClass(bool& outValid) {
        outValid = false;
        if (!sharedGameManager) return Ball::Classification::SOLID;
        Ball::Classification cls = sharedGameManager.getPlayerClassification();
        if (cls == Ball::Classification::SOLID || cls == Ball::Classification::STRIPE) {
            outValid = true;
            return cls;
        }
        return Ball::Classification::SOLID;  // fallback (tidak valid)
    }

    // Hitung jumlah bola milik sendiri yang masih di meja
    static int CountMyBallsOnTable(Ball::Classification myClass) {
        int count = 0;
        if (!gPrediction) return 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myClass && b.originalOnTable)
                count++;
        }
        return count;
    }

    // Skor satu angle — return nilai skor, negatif = invalid/foul
    static int ScoreAngle(double angle, Ball::Classification target,
                          Ball::Classification opponent, bool only8BLeft) {
        gPrediction->determineShotResult(true, angle);

        auto& cueBall   = gPrediction->guiData.balls[0];
        auto& eightBall = gPrediction->guiData.balls[8];

        // Scratch = foul
        if (!cueBall.onTable) return -9999;

        // Harus ada first hit
        if (!gPrediction->guiData.collision.firstHitBall) return -9000;

        // First hit HARUS bola target sendiri
        auto* firstHit = gPrediction->guiData.collision.firstHitBall;
        if (firstHit->classification != target) return -8000;

        // Jika belum waktunya 8-ball, jangan pocket 8-ball
        if (!only8BLeft && !eightBall.onTable) return -7000;

        // Hitung skor
        int score     = 0;
        int ownPotted = 0;

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;
            if (ball.onTable) continue;       // bola masih di meja setelah simulasi

            if (ball.classification == target) {
                score += 20;
                ownPotted++;
                // Bonus jika pocket lebih dari satu
                if (ownPotted > 1) score += 10;
            } else if (ball.classification == opponent) {
                // Pocket bola lawan = foul
                score -= 40;
            }
        }

        // Minimal harus pocket satu bola sendiri agar valid
        if (ownPotted <= 0) return -1;

        return score;
    }

    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        bool classValid = false;
        Ball::Classification myClass = DetectMyClass(classValid);

        if (!classValid) { lastHadShot = false; return; }

        bool only8BLeft = (CountMyBallsOnTable(myClass) == 0);
        Ball::Classification target   = only8BLeft
            ? Ball::Classification::EIGHT_BALL : myClass;
        Ball::Classification opponent = (myClass == Ball::Classification::SOLID)
            ? Ball::Classification::STRIPE : Ball::Classification::SOLID;

        // ── Phase 1: Coarse scan ───────────────────────────────────────────
        double bestAngle     = startAngle;
        int    bestScore     = -9999;
        int    bestBallIdx   = -1;
        int    bestPocketIdx = -1;

        double scanAngle = startAngle;
        int    maxIter   = (int)(MAX_ANGLE_RADIANS / COARSE_STEP) + 4;

        for (int iter = 0; iter < maxIter; iter++) {
            int sc = ScoreAngle(scanAngle, target, opponent, only8BLeft);
            if (sc > 0) {
                // Ambil pocket bola target pertama yang masuk
                int firstBall   = -1;
                int firstPocket = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (!ball.originalOnTable || ball.onTable) continue;
                    if (ball.classification == target) {
                        firstBall   = i;
                        firstPocket = ball.pocketIndex;
                        break;
                    }
                }
                // Filter pocket: jika user sudah pilih pocket, skip angle ini jika bukan pocket itu
                if (g_selectedPocket8 >= 0 && firstPocket != g_selectedPocket8) {
                    scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                    scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
                    if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
                    continue;
                }
                if (sc > bestScore) {
                    bestScore     = sc;
                    bestAngle     = scanAngle;
                    bestBallIdx   = firstBall;
                    bestPocketIdx = firstPocket;
                }
            }
            scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
        }

        if (bestScore < 0) { lastTargetBall = -1; lastHadShot = false; return; }

        // ── Phase 2: Fine refinement ───────────────────────────────────────
        double refineStart = bestAngle - COARSE_STEP;
        double refineEnd   = bestAngle + COARSE_STEP;
        double fineAngle   = refineStart;

        while (true) {
            fineAngle = NumberUtils::normalizeDoublePrecision(
                fmod(fineAngle + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS));
            int sc = ScoreAngle(fineAngle, target, opponent, only8BLeft);
            if (sc > 0) {
                int firstBall   = -1;
                int firstPocket = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (!ball.originalOnTable || ball.onTable) continue;
                    if (ball.classification == target) {
                        firstBall   = i;
                        firstPocket = ball.pocketIndex;
                        break;
                    }
                }
                if (g_selectedPocket8 >= 0 && firstPocket != g_selectedPocket8) {
                    fineAngle += FINE_STEP;
                    if (fineAngle - refineEnd > 0.0) break;
                    continue;
                }
                if (sc > bestScore) {
                    bestScore     = sc;
                    bestAngle     = fineAngle;
                    bestBallIdx   = firstBall;
                    bestPocketIdx = firstPocket;
                }
            }
            fineAngle += FINE_STEP;
            if (fineAngle - refineEnd > 0.0) break;
        }

        lastTargetBall   = bestBallIdx;
        lastTargetPocket = bestPocketIdx;
        lastHadShot      = true;
        vg.mAimAngle(bestAngle);
    }
}
