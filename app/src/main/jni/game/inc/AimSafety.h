#pragma once

// ── Safety Shot / Snooker AI — 8 Ball Pool ────────────────────────────────────
// Strategi:
//   Scan semua angle → cari tembakan yang:
//   (a) Legal: hit bola kita duluan, cue tidak scratch
//   (b) Maksimalkan bola musuh yang "blocked" — tidak punya line-of-sight
//       langsung ke cue ball setelah tembakan, karena bola kita menghalangi
//   (c) Bonus: pocket ≥1 bola kita sekaligus
//
// LOS check: segment-circle intersection dari posisi akhir cue ke setiap
// bola musuh, melewati posisi akhir semua bola kita yang masih di meja.
// ─────────────────────────────────────────────────────────────────────────────

namespace AimSafety {

    int   lastBlockedCount  = 0;
    bool  lastHadShot       = false;
    int   lastTargetBall    = -1;
    int   lastTargetPocket  = -1;

    static constexpr double SCAN_STEP    = MIN_ANGLE_STEP_RADIANS * 0.50; // ~0.5°
    static constexpr double BLOCK_RADIUS = BALL_RADIUS * 1.65; // margin blocker

    // Spin presets — kontrol posisi akhir cue ball
    static constexpr int SPIN_COUNT = 5;
    static const Vec2d SPINS[SPIN_COUNT] = {
        Vec2d( 0.0,  0.0),   // neutral
        Vec2d( 0.0, -0.5),   // back spin ringan (cue lebih pendek)
        Vec2d( 0.0, -0.9),   // back spin kuat (stop ball)
        Vec2d( 0.0,  0.4),   // top spin (cue lanjut lebih jauh)
        Vec2d( 0.0, -0.3),   // back sangat ringan
    };

    static inline void WriteEnglish(double sx, double sy) {
        if (!sharedGameManager) return;
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        sx = sx >  1.0 ?  1.0 : (sx < -1.0 ? -1.0 : sx);
        sy = sy >  1.0 ?  1.0 : (sy < -1.0 ? -1.0 : sy);
        vec.mEnglish(Vec2d(sx, sy));
    }

    // ── Segment–Circle Intersection ───────────────────────────────────────────
    // Returns true jika segmen [P1→P2] melewati dalam radius `r` dari pusat `C`
    static bool SegmentHitsCircle(const Point2D& P1, const Point2D& P2,
                                   const Point2D& C,  double r) {
        double dx = P2.x - P1.x,  dy = P2.y - P1.y;
        double fx = P1.x -  C.x,  fy = P1.y -  C.y;
        double a  = dx*dx + dy*dy;
        if (a < 1e-12) return false;
        double bv = 2.0*(fx*dx + fy*dy);
        double c  = fx*fx + fy*fy - r*r;
        double disc = bv*bv - 4.0*a*c;
        if (disc < 0.0) return false;
        double sq = sqrt(disc);
        double t1 = (-bv - sq) / (2.0*a);
        double t2 = (-bv + sq) / (2.0*a);
        // interseksi jika salah satu t dalam [0,1] atau segmen di dalam lingkaran
        return (t1 >= 0.0 && t1 <= 1.0) ||
               (t2 >= 0.0 && t2 <= 1.0) ||
               (t1 < 0.0  && t2 > 1.0);
    }

    // ── Hitung berapa bola musuh yang ter-block ───────────────────────────────
    // "Blocked" = ada minimal 1 bola kita yang menghalangi garis lurus dari
    // posisi akhir cue ke bola musuh tersebut
    static int CountBlockedEnemies(const Point2D& cuePos,
                                    Ball::Classification myClass,
                                    Ball::Classification opp) {
        int blocked = 0;
        int n = gPrediction->guiData.ballsCount;
        for (int ei = 1; ei < n; ei++) {
            auto& enemy = gPrediction->guiData.balls[ei];
            if (enemy.classification != opp) continue;
            if (!enemy.onTable) continue;

            for (int bi = 1; bi < n; bi++) {
                auto& blocker = gPrediction->guiData.balls[bi];
                if (blocker.classification != myClass) continue;
                if (!blocker.onTable) continue;
                if (SegmentHitsCircle(cuePos, enemy.predictedPosition,
                                      blocker.predictedPosition, BLOCK_RADIUS)) {
                    blocked++;
                    break; // cukup 1 blocker per musuh
                }
            }
        }
        return blocked;
    }

    // ── Kandidat tembakan ─────────────────────────────────────────────────────
    struct Cand {
        double angle;
        Vec2d  spin;
        double score;
        int    blocked;
        int    potted;
        int    ballIdx;
        int    pocketIdx;
    };

    // ── Aim() — entry point ───────────────────────────────────────────────────
    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();
        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double power      = vc.getShotPower();

        auto myClass = sharedGameManager.getPlayerClassification();
        if (myClass != Ball::Classification::SOLID &&
            myClass != Ball::Classification::STRIPE) {
            lastHadShot = false; return;
        }
        auto opp = (myClass == Ball::Classification::SOLID)
                    ? Ball::Classification::STRIPE
                    : Ball::Classification::SOLID;

        int maxIter = (int)(MAX_ANGLE_RADIANS / SCAN_STEP) + 4;
        Cand best   = {startAngle, Vec2d(0,0), -9999.0, 0, 0, -1, -1};

        double scanAngle = startAngle;
        for (int iter = 0; iter < maxIter; iter++) {

            for (int spi = 0; spi < SPIN_COUNT; spi++) {
                Vec2d sp = SPINS[spi];
                gPrediction->determineShotResult(true, scanAngle, power, sp);

                auto& cue = gPrediction->guiData.balls[0];

                // 1. Cue tidak scratch
                if (!cue.onTable) continue;
                // 2. Hit bola kita duluan
                if (!gPrediction->guiData.collision.firstHitBall) continue;
                if (gPrediction->guiData.collision.firstHitBall->classification != myClass) continue;

                // 3. Hitung bola kita yang masuk + cek tidak potong bola 8
                int potted = 0;
                int bIdx = -1, bPkt = -1;
                bool eight_potted = false;
                int n = gPrediction->guiData.ballsCount;
                for (int i = 1; i < n; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (!b.originalOnTable) continue;
                    if (b.classification == Ball::Classification::EIGHT_BALL && !b.onTable) {
                        eight_potted = true; break;
                    }
                    if (b.classification == myClass && !b.onTable) {
                        potted++;
                        if (bIdx < 0) { bIdx = i; bPkt = b.pocketIndex; }
                    }
                }
                if (eight_potted) continue;

                // 4. Hitung blocked enemies
                int blocked = CountBlockedEnemies(cue.predictedPosition, myClass, opp);

                // Tidak ada nilai sama sekali → skip
                if (blocked == 0 && potted == 0) continue;

                // 5. Score: blocked lebih penting dari potted
                double score = (double)blocked * 30.0 + (double)potted * 15.0;

                if (score > best.score)
                    best = {scanAngle, sp, score, blocked, potted, bIdx, bPkt};
            }

            scanAngle = fmod(scanAngle + SCAN_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < SCAN_STEP * 0.5) break;
        }

        // ── Apply ─────────────────────────────────────────────────────────────
        if (best.score > -9000.0 && (best.blocked > 0 || best.potted > 0)) {
            lastHadShot      = true;
            lastBlockedCount = best.blocked;
            lastTargetBall   = best.ballIdx;
            lastTargetPocket = best.pocketIdx;

            vg.mAimAngle(best.angle);
            WriteEnglish(best.spin.x, best.spin.y);

            if (best.pocketIdx >= 0 && sharedGameManager)
                sharedGameManager.nominatePocket(best.pocketIdx);

            // Force full recalc untuk DrawESP visual lines
            prevAngle = -99999.0;
            gPrediction->determineShotResult(false, best.angle, power, best.spin);
        } else {
            lastHadShot      = false;
            lastBlockedCount = 0;
            lastTargetBall   = -1;
            lastTargetPocket = -1;
        }
    }

} // namespace AimSafety
