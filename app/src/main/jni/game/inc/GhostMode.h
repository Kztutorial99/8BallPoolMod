#pragma once

// ══════════════════════════════════════════════════════════════════════════════
// INJECT GHOST MODE — EXTREME BRUTAL EDITION
// ══════════════════════════════════════════════════════════════════════════════
//
// 4 THREAD PARALEL inject sekaligus:
//   Thread A — POWER LOCKDOWN     : tulis mPower=0 berulang secepat mungkin
//   Thread B — AIM ANNIHILATOR    : hancurkan aim angle tiap cycle
//   Thread C — PHYSICS FREEZE     : zero semua ball velocity + lock cue position
//   Thread D — SPIN CHAOS ENGINE  : extreme english + power glitch
//
// Speed:
//   fSpeed 1   → sleep 15000µs (~67 cycle/s per thread)
//   fSpeed 100 → sleep 0 + sched_yield (~MAX hardware throughput)
//
// State coverage:
//   State 7 = giliran musuh (shooting)
//   State 8 = giliran musuh ball-in-hand (geser posisi)
//   Kedua state dicover → musuh tidak bisa apa-apa sama sekali
// ══════════════════════════════════════════════════════════════════════════════

#include <atomic>
#include <thread>
#include <unistd.h>
#include <sched.h>
#include <cmath>
#include <cstring>

namespace GhostMode {

    // ── Public state ───────────────────────────────────────────────────────────
    bool  bActive    = false;
    float fSpeed     = 1.0f;    // 1.0 = Min, 100.0 = Ultra Max
    int   iCtrlPocket = 0;      // pocket target untuk ctrl-aim (0-5)

    // ── Internal atomics ───────────────────────────────────────────────────────
    static std::atomic<bool>   s_alive { false };   // master kill switch
    static std::atomic<int>    s_activeThr { 0 };   // thread count aktif

    // ── Frozen cue-ball position cache ────────────────────────────────────────
    static Vec2d   s_frozenCuePos { 0.0, 0.0 };
    static bool    s_cuePosLocked = false;

    // ── Helper: apakah giliran musuh (state 7 atau 8) ─────────────────────────
    static inline bool IsEnemyTurn() {
        if (!sharedGameManager) return false;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return false;
        int sid = gsm.getCurrentStateId();
        return sid == 7 || sid == 8;
    }

    // ── Helper: sleep berdasarkan fSpeed ──────────────────────────────────────
    // fSpeed 1   → 15000µs
    // fSpeed 10  →  1500µs
    // fSpeed 50  →   300µs
    // fSpeed 85  →    50µs
    // fSpeed 100 →  sched_yield (0 sleep, yield CPU sebentar lalu lanjut)
    static inline void SpeedSleep(float spd) {
        if (spd >= 98.0f) {
            sched_yield();          // Ultra Max: tidak ada sleep sama sekali
        } else {
            int us = (int)(15000.0f / spd);
            if (us < 10) us = 10;
            usleep((useconds_t)us);
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // THREAD A — POWER LOCKDOWN
    // Tulis mPower = 0.0 LIMA KALI per cycle → tidak ada celah engine commit shot
    // ════════════════════════════════════════════════════════════════════════
    static void ThreadA_PowerLock() {
        s_activeThr.fetch_add(1);
        while (s_alive.load()) {
            if (bActive && IsEnemyTurn()) {
                auto vc = sharedGameManager.mVisualCue();
                if (vc) {
                    // Multi-write: tulis 5x berturut-turut tanpa jeda
                    vc.mPower(0.0);
                    vc.mPower(0.0);
                    vc.mPower(0.0);
                    vc.mPower(0.0);
                    vc.mPower(0.0);
                }
            }
            SpeedSleep(fSpeed);
        }
        s_activeThr.fetch_sub(1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // THREAD B — AIM ANNIHILATOR
    // Lock aim angle ke nilai sampah fixed + micro-jitter ekstrim setiap cycle
    // Tulis 5x berturut-turut → game tidak punya waktu untuk override
    // ════════════════════════════════════════════════════════════════════════
    static void ThreadB_AimAnnihilate() {
        s_activeThr.fetch_add(1);
        static volatile int aTick = 0;
        // Pocket angles — arahkan ke pocket yang dipilih user
        static const double pocketAngles[6] = {
            2.356194490192345,   // top-left
            1.5707963267948966,  // top-center
            0.7853981633974483,  // top-right
            5.497787143782138,   // bot-right
            4.71238898038469,    // bot-center
            3.9269908169872414   // bot-left
        };

        while (s_alive.load()) {
            if (bActive && IsEnemyTurn()) {
                auto vc = sharedGameManager.mVisualCue();
                if (vc) {
                    auto vg = vc.mVisualGuide();
                    if (vg) {
                        aTick++;
                        // Alternating chaos: tiap 2 tick ganti antara 2 nilai ekstrim
                        // → aim "terpental" bolak-balik tidak ada posisi stabil
                        double v1 = pocketAngles[iCtrlPocket >= 0 && iCtrlPocket <= 5 ? iCtrlPocket : 0];
                        // Offset 180° dari target → selalu salah arah
                        double v2 = fmod(v1 + 3.14159265358979323846 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                        // Tambah jitter berdasarkan tick
                        double jitter = (aTick % 7) * 0.18;
                        double pick = (aTick % 2 == 0) ? v2 + jitter : v1 - jitter;
                        pick = NumberUtils::normalizeDoublePrecision(fmod(pick + MAX_ANGLE_RADIANS * 4.0, MAX_ANGLE_RADIANS));

                        // Multi-write 5x
                        vg.mAimAngle(pick);
                        vg.mAimAngle(pick);
                        vg.mAimAngle(pick);
                        vg.mAimAngle(pick);
                        vg.mAimAngle(pick);

                        // Power selalu 0 juga dari thread ini (redundant = lebih brutal)
                        vc.mPower(0.0);
                        vc.mPower(0.0);
                    }
                }
            }
            SpeedSleep(fSpeed);
        }
        s_activeThr.fetch_sub(1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // THREAD C — PHYSICS FREEZE
    // Zero semua ball velocity → bola tidak bergerak
    // Lock cue ball position → musuh tidak bisa geser (state 8 / ball-in-hand)
    // ════════════════════════════════════════════════════════════════════════
    static void ThreadC_PhysicsFreeze() {
        s_activeThr.fetch_add(1);
        while (s_alive.load()) {
            if (bActive && IsEnemyTurn()) {
                Table table = sharedGameManager.mTable;
                if (table) {
                    auto& balls = table.mBalls();
                    if (balls) {
                        int sid = sharedGameManager.mStateManager().getCurrentStateId();
                        bool isBallInHand = (sid == 8);

                        for (size_t i = 0; i < balls.Count && i < 16; i++) {
                            Ball ball(balls[i]);
                            if (!ball.instance) continue;

                            // Zero velocity → bola tidak bisa bergerak
                            ball.velocity(Vec2d(0.0, 0.0));
                            ball.velocity(Vec2d(0.0, 0.0));   // double-write

                            // Zero spin agar tidak ada angular momentum
                            ball.spin(Vec3d(0.0, 0.0, 0.0));

                            // Cue ball: lock position saat ball-in-hand (state 8)
                            if (ball.classification() == Ball::Classification::CUE_BALL) {
                                if (isBallInHand) {
                                    if (!s_cuePosLocked) {
                                        // Rekam posisi cue ball pertama kali
                                        s_frozenCuePos = ball.position();
                                        s_cuePosLocked = true;
                                    } else {
                                        // Tulis balik posisi yang direkam → musuh tidak bisa pindahkan
                                        ball.position(s_frozenCuePos);
                                        ball.position(s_frozenCuePos);   // double-write
                                    }
                                } else {
                                    // Tidak ball-in-hand → reset lock
                                    s_cuePosLocked = false;
                                }
                            }
                        }
                    }
                }
            } else {
                // Tidak giliran musuh → reset cue lock
                s_cuePosLocked = false;
            }
            SpeedSleep(fSpeed);
        }
        s_activeThr.fetch_sub(1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // THREAD D — SPIN CHAOS ENGINE
    // Extreme english + power glitch + force foul setiap cycle
    // ════════════════════════════════════════════════════════════════════════
    static void ThreadD_SpinChaos() {
        s_activeThr.fetch_add(1);
        static volatile int dTick = 0;
        while (s_alive.load()) {
            if (bActive && IsEnemyTurn()) {
                dTick++;

                // English: alternating max side spin + back/top
                auto vec = sharedGameManager.mVisualEnglishControl();
                if (vec) {
                    double sx = (dTick % 2 == 0) ?  0.99 : -0.99;
                    double sy = (dTick % 3 == 0) ? -0.99 :  0.99;
                    vec.mEnglish(Vec2d(sx, sy));
                    vec.mEnglish(Vec2d(sx, sy));   // double-write
                    vec.mEnglish(Vec2d(sx, sy));   // triple-write
                }

                // Force foul: aim 90° off + power rendah
                auto vc = sharedGameManager.mVisualCue();
                if (vc) {
                    // Power alternating: kalau lolos dari Thread A, tangkap di sini
                    vc.mPower(0.0);
                    vc.mPower(0.0);

                    auto vg = vc.mVisualGuide();
                    if (vg) {
                        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
                        // Rotate 90° setiap tick ganjil
                        double offset = (dTick % 2 == 0)
                            ?  1.5707963267948966   // +90°
                            : -1.5707963267948966;  // -90°
                        double bad = fmod(cur + offset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
                        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(bad));
                        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(bad));
                    }
                }
            }
            SpeedSleep(fSpeed);
        }
        s_activeThr.fetch_sub(1);
    }

    // ════════════════════════════════════════════════════════════════════════
    // PUBLIC API
    // ════════════════════════════════════════════════════════════════════════

    // Aktifkan Ghost Mode → spawn 4 thread parallel
    void SetActive(bool active) {
        bActive = active;

        if (active && !s_alive.load()) {
            s_cuePosLocked = false;
            s_alive = true;
            // Spawn 4 thread sekaligus
            std::thread(ThreadA_PowerLock).detach();
            std::thread(ThreadB_AimAnnihilate).detach();
            std::thread(ThreadC_PhysicsFreeze).detach();
            std::thread(ThreadD_SpinChaos).detach();
        } else if (!active) {
            s_alive = false;   // semua thread berhenti sendiri
            s_cuePosLocked = false;
        }
    }

    // Per-frame safety net: jika thread mati karena exception, restart
    void Update() {
        if (!bActive) return;
        // Auto-restart jika semua thread mati tapi Ghost masih aktif
        if (s_alive.load() && s_activeThr.load() == 0) {
            s_alive = false;
            SetActive(true);
        }
    }

    void Shutdown() {
        bActive = false;
        s_alive = false;
        s_cuePosLocked = false;
    }

    // Speed label untuk UI
    static const char* GetSpeedLabel(float spd) {
        if (spd <= 1.5f)  return "MIN  (~67 cycle/s)";
        if (spd <= 5.0f)  return "SLOW (~300 cycle/s)";
        if (spd <= 15.0f) return "NORMAL (~1K cycle/s)";
        if (spd <= 35.0f) return "FAST (~3.5K cycle/s)";
        if (spd <= 60.0f) return "HIGH (~10K cycle/s)";
        if (spd <= 85.0f) return "EXTREME (~50K cycle/s)";
        return "ULTRA MAX — NO SLEEP !!!";
    }

} // namespace GhostMode
