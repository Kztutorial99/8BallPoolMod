#pragma once

// ── Aim Predict Pocket Lock — 8 Ball Pool ─────────────────────────────────────
// Strategi multi-fase:
//  Phase 1  : Coarse scan 0.5° step, spin netral → kumpulkan semua kandidat valid
//  Phase 1b : Angle yang bola masuk pocket tapi cue scratch → coba back-spin variants
//  Phase 2  : Fine refinement ±3 × coarse_step @ 0.15° step pada SEMUA kandidat
//  Phase 3  : Proximity fallback — jika tidak ada jalur langsung, arahkan bola
//             sedekat mungkin ke pocket target dan tetap aim
//  Phase 4  : Apply angle + spin terbaik, fix visual cache (force full position recalc)
// ─────────────────────────────────────────────────────────────────────────────

namespace AimLockTarget {

    bool  bActive          = false;
    int   lastTargetBall   = -1;
    int   lastTargetPocket = -1;
    bool  lastHadShot      = false;
    float lastProximityPct = 0.0f;   // 0–100: seberapa dekat ke pocket target (100 = exact)

    // Step scan lebih halus dari default untuk tidak melewatkan celah sempit
    static constexpr double COARSE_STEP = MIN_ANGLE_STEP_RADIANS * 0.50; // ~0.5°
    static constexpr double FINE_STEP   = MIN_ANGLE_STEP_RADIANS * 0.15; // ~0.15°

    // Back-spin presets untuk recovery scratch
    // Format Vec2d: x = side spin, y = top/back spin (negatif = back spin)
    static constexpr int BACK_SPIN_COUNT = 7;
    static const Vec2d   BACK_SPINS[BACK_SPIN_COUNT] = {
        Vec2d( 0.0,  -0.35),   // back ringan
        Vec2d( 0.0,  -0.55),   // back sedang
        Vec2d( 0.0,  -0.75),   // back kuat
        Vec2d( 0.0,  -1.00),   // back penuh (stop ball)
        Vec2d( 0.35, -0.45),   // back + side kanan
        Vec2d(-0.35, -0.45),   // back + side kiri
        Vec2d( 0.0,  -0.90),   // back sangat kuat
    };

    // WriteEnglish inline — tidak bergantung AutoEnglish.h include order
    static inline void WriteEnglish(double sx, double sy) {
        if (!sharedGameManager) return;
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        if (sx >  1.0) sx =  1.0; if (sx < -1.0) sx = -1.0;
        if (sy >  1.0) sy =  1.0; if (sy < -1.0) sy = -1.0;
        vec.mEnglish(Vec2d(sx, sy));
    }

    // Struktur kandidat tembakan
    struct ShotCandidate {
        double angle;
        Vec2d  spin;
        double score;
        int    ballIdx;
        int    pocketIdx;
    };

    // ── Helpers ───────────────────────────────────────────────────────────────

    static Ball::Classification DetectMyClass(bool& outValid) {
        outValid = false;
        if (!sharedGameManager) return Ball::Classification::SOLID;
        auto cls = sharedGameManager.getPlayerClassification();
        if (cls == Ball::Classification::SOLID || cls == Ball::Classification::STRIPE) {
            outValid = true;
            return cls;
        }
        return Ball::Classification::SOLID;
    }

    static int CountMyBallsOnTable(Ball::Classification myClass) {
        int count = 0;
        if (!gPrediction) return 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myClass && b.originalOnTable) count++;
        }
        return count;
    }

    // ── EvalShot ─────────────────────────────────────────────────────────────
    // Jalankan simulasi 1 tembakan. Return: skor (>0 valid, <=0 invalid/foul).
    // outBall / outPocket = bola + pocket paling relevan yang ditemukan.
    // Ketika forcePocket >= 0, bonus +500 jika bola masuk ke pocket itu.
    // outProximity [0..1]: seberapa dekat bola terbaik ke forcePocket (fallback).
    static double EvalShot(double angle, Vec2d spin,
                            Ball::Classification target,
                            Ball::Classification opponent,
                            bool only8BLeft,
                            int  forcePocket,
                            int& outBall, int& outPocket,
                            double& outProximity) {
        double power = sharedGameManager.mVisualCue().getShotPower();
        gPrediction->determineShotResult(true, angle, power, spin);

        outBall = -1; outPocket = -1; outProximity = 0.0;

        auto& cueBall   = gPrediction->guiData.balls[0];
        auto& eightBall = gPrediction->guiData.balls[8];

        if (!cueBall.onTable)                                               return -9999.0;
        if (!gPrediction->guiData.collision.firstHitBall)                  return -9000.0;
        if (gPrediction->guiData.collision.firstHitBall->classification != target)
                                                                            return -8000.0;
        if (!only8BLeft && !eightBall.onTable)                              return -7000.0;

        const auto& pockets  = getPockets();
        double score         = 0.0;
        int    ownPotted     = 0;
        bool   exactHit      = false;

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;

            if (ball.classification == target) {
                if (!ball.onTable) {
                    ownPotted++;
                    score += 20.0;
                    if (ownPotted > 1) score += 10.0;

                    bool isTargetPocket = (forcePocket >= 0 && ball.pocketIndex == forcePocket);
                    if (outBall < 0 || isTargetPocket) {
                        outBall   = i;
                        outPocket = ball.pocketIndex;
                    }
                    if (isTargetPocket) {
                        score   += 500.0;
                        exactHit = true;
                    }
                } else if (forcePocket >= 0 && !exactHit) {
                    // Bola masih di meja — ukur jarak ke pocket target untuk fallback
                    double bx   = ball.predictedPosition.x;
                    double by   = ball.predictedPosition.y;
                    double px   = pockets[forcePocket].x;
                    double py   = pockets[forcePocket].y;
                    double dist = sqrt((bx-px)*(bx-px) + (by-py)*(by-py));
                    double prox = 1.0 - (dist / 284.0);   // diagonal meja ~284
                    if (prox < 0.0) prox = 0.0;
                    if (prox > outProximity) outProximity = prox;
                }
            } else if (ball.classification == opponent && !ball.onTable) {
                score -= 40.0;
            }
        }

        if (ownPotted == 0) {
            return (outProximity > 0.0) ? 0.0 : -1.0;
        }
        return score;
    }

    // ── Aim() — fungsi utama ─────────────────────────────────────────────────
    void Aim() {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();
        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());

        bool classValid = false;
        auto myClass = DetectMyClass(classValid);
        if (!classValid) { lastHadShot = false; return; }

        bool only8BLeft = (CountMyBallsOnTable(myClass) == 0);
        auto target   = only8BLeft ? Ball::Classification::EIGHT_BALL : myClass;
        auto opponent = (myClass == Ball::Classification::SOLID)
                        ? Ball::Classification::STRIPE : Ball::Classification::SOLID;

        int   forcePocket = PocketSelector::Get();
        Vec2d neutralSpin(0.0, 0.0);
        int   maxIter = (int)(MAX_ANGLE_RADIANS / COARSE_STEP) + 4;

        static const int MAX_CAND    = 32;
        static const int MAX_SCRATCH = 24;

        ShotCandidate coarseCands[MAX_CAND];
        int nCands = 0;

        struct ScratchEntry { double angle; int ballIdx; int pocketIdx; };
        ScratchEntry scratchAngles[MAX_SCRATCH];
        int nScratch = 0;

        ShotCandidate bestFallback    = {startAngle, neutralSpin, -9999.0, -1, -1};
        double        bestFallbackProx = 0.0;

        auto AddCandidate = [&](ShotCandidate c) {
            if (nCands < MAX_CAND) {
                coarseCands[nCands++] = c;
            } else {
                int worst = 0;
                for (int k = 1; k < MAX_CAND; k++)
                    if (coarseCands[k].score < coarseCands[worst].score) worst = k;
                if (c.score > coarseCands[worst].score) coarseCands[worst] = c;
            }
        };

        // ── Phase 1: Coarse scan (spin netral, langkah 0.5°) ─────────────────
        double scanAngle = startAngle;
        for (int iter = 0; iter < maxIter; iter++) {
            int ob, op; double prox;
            double sc = EvalShot(scanAngle, neutralSpin, target, opponent,
                                 only8BLeft, forcePocket, ob, op, prox);

            if (sc > 0.0) {
                bool exactPocket = (forcePocket < 0 || op == forcePocket);
                if (exactPocket) {
                    AddCandidate({scanAngle, neutralSpin, sc, ob, op});
                }
                if (sc > bestFallback.score && ob >= 0) {
                    bestFallback     = {scanAngle, neutralSpin, sc, ob, op};
                    bestFallbackProx = 0.8;
                }
            } else if (sc == -9999.0 && forcePocket >= 0) {
                // Scratch — cek apakah bola yang masuk adalah target pocket
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (!ball.originalOnTable || ball.onTable) continue;
                    if (ball.classification == target && ball.pocketIndex == forcePocket) {
                        if (nScratch < MAX_SCRATCH)
                            scratchAngles[nScratch++] = {scanAngle, i, ball.pocketIndex};
                        break;
                    }
                }
            } else if (sc == 0.0 && prox > bestFallbackProx) {
                bestFallback     = {scanAngle, neutralSpin, sc, ob, op};
                bestFallbackProx = prox;
            }

            scanAngle = fmod(scanAngle + COARSE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
            if (iter > 0 && std::abs(scanAngle - startAngle) < COARSE_STEP * 0.5) break;
        }

        // ── Phase 1b: Spin recovery untuk scratch-rejected angles ─────────────
        if (forcePocket >= 0 && nScratch > 0) {
            for (int si = 0; si < nScratch; si++) {
                bool fixed = false;
                for (int spi = 0; spi < BACK_SPIN_COUNT && !fixed; spi++) {
                    int ob, op; double prox;
                    double sc = EvalShot(scratchAngles[si].angle, BACK_SPINS[spi],
                                         target, opponent, only8BLeft, forcePocket,
                                         ob, op, prox);
                    if (sc > 0.0 && op == forcePocket) {
                        AddCandidate({scratchAngles[si].angle, BACK_SPINS[spi], sc, ob, op});
                        fixed = true;
                    }
                }
            }
        }

        // ── Phase 2: Fine refinement pada SEMUA kandidat ──────────────────────
        ShotCandidate best = {startAngle, neutralSpin, -9999.0, -1, -1};

        for (int ci = 0; ci < nCands; ci++) {
            double refStart = coarseCands[ci].angle - COARSE_STEP * 3.0;
            double refEnd   = coarseCands[ci].angle + COARSE_STEP * 3.0;
            for (double fa = refStart; fa <= refEnd + FINE_STEP * 0.5; fa += FINE_STEP) {
                double nfa = NumberUtils::normalizeDoublePrecision(
                    fmod(fa + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS));
                int ob, op; double prox;
                double sc = EvalShot(nfa, coarseCands[ci].spin,
                                     target, opponent, only8BLeft, forcePocket,
                                     ob, op, prox);
                bool exactPocket = (forcePocket < 0 || op == forcePocket);
                if (sc > best.score && exactPocket)
                    best = {nfa, coarseCands[ci].spin, sc, ob, op};
            }
        }

        // ── Phase 3: Fallback — tidak ada jalur langsung ke pocket ────────────
        if (best.score < 0.0) {
            if (bestFallback.ballIdx >= 0 || bestFallbackProx > 0.3) {
                lastTargetBall   = bestFallback.ballIdx;
                lastTargetPocket = bestFallback.pocketIdx;
                lastHadShot      = false;
                lastProximityPct = (float)(bestFallbackProx * 100.0);

                if (bestFallback.score > 0.0 || bestFallback.ballIdx >= 0) {
                    vg.mAimAngle(bestFallback.angle);
                    WriteEnglish(0.0, -0.30);
                    prevAngle = -99999.0;
                    gPrediction->determineShotResult(false, bestFallback.angle,
                        sharedGameManager.mVisualCue().getShotPower(), bestFallback.spin);
                    int pkt = (forcePocket >= 0) ? forcePocket
                            : (bestFallback.pocketIdx >= 0 ? bestFallback.pocketIdx : 0);
                    sharedGameManager.nominatePocket(pkt);
                }
            } else {
                lastTargetBall   = -1;
                lastHadShot      = false;
                lastProximityPct = 0.0f;
            }
            return;
        }

        // ── Phase 4: Apply hasil terbaik ──────────────────────────────────────
        lastTargetBall   = best.ballIdx;
        lastTargetPocket = best.pocketIdx;
        lastHadShot      = true;
        lastProximityPct = 100.0f;

        vg.mAimAngle(best.angle);
        WriteEnglish(best.spin.x, best.spin.y);

        int pktToNominate = (forcePocket >= 0) ? forcePocket : best.pocketIdx;
        if (pktToNominate >= 0 && sharedGameManager)
            sharedGameManager.nominatePocket(pktToNominate);

        // ── FIX CRITICAL: Invalidasi cache prevAngle ──────────────────────────
        // determineShotResult(true, bestAngle) menyimpan prevAngle=bestAngle fastCalc=true.
        // DrawESP memanggil determineShotResult(false) dengan angle sama → cache hit →
        // fastCalc tidak di-reset → posisi bola TIDAK direkam → garis visual KOSONG.
        // Fix: set prevAngle ke nilai tidak valid → paksa full recalc fastCalc=false.
        prevAngle = -99999.0;
        gPrediction->determineShotResult(false, best.angle,
            sharedGameManager.mVisualCue().getShotPower(), best.spin);
    }

} // namespace AimLockTarget
