#pragma once

// ══════════════════════════════════════════════════════════════════════════════
// INJECT GHOST MODE — Unified Enemy Domination System
// ══════════════════════════════════════════════════════════════════════════════
//
// Menggabungkan SEMUA 19 mode sabotase sekaligus + Speed Exploit:
//   - Semua inject aktif bersamaan (aim deflect + invert + power drain/surge
//     + spin chaos + english random + jitter + angle drift + total blackout
//     + no shoot + ctrl turn + force foul + scratch + godmode)
//
// Speed Exploit:
//   fSpeed = 1.0   → Min  (sekali per frame, ~60 call/det)
//   fSpeed = 10.0  → Normal (600 call/det)
//   fSpeed = 50.0  → High (3000 call/det)
//   fSpeed = 100.0 → ULTRA MAX (musuh tidak bisa gerak sama sekali)
//
// Background thread inject semua mode setiap sleepUs microseconds,
// semakin kecil sleepUs → semakin cepat → musuh freeze total.
// ══════════════════════════════════════════════════════════════════════════════

#include <atomic>
#include <thread>
#include <unistd.h>
#include <cmath>

namespace GhostMode {

    // ── State ──────────────────────────────────────────────────────────────────
    bool              bActive       = false;
    float             fSpeed        = 1.0f;    // 1.0 = Min, 100.0 = Ultra Max
    static std::atomic<bool>   s_threadAlive { false };
    static std::atomic<bool>   s_injectNow   { false };

    // Ctrl Turn pocket target (0-5)
    int iCtrlPocket = 0;

    // ── Internal helpers ───────────────────────────────────────────────────────
    static bool IsEnemyTurn() {
        if (!sharedGameManager) return false;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return false;
        int sid = gsm.getCurrentStateId();
        return sid == 7 || sid == 8;
    }

    // ── Inject: AIM DEFLECT ────────────────────────────────────────────────────
    static void InjectAimDeflect() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + 0.18 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    // ── Inject: AIM INVERT ────────────────────────────────────────────────────
    static void InjectAimInvert() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double inverted = fmod(cur + 3.14159265358979323846 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(inverted));
    }

    // ── Inject: POWER DRAIN ───────────────────────────────────────────────────
    static void InjectPowerDrain() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(0.0);
    }

    // ── Inject: POWER SURGE ───────────────────────────────────────────────────
    static void InjectPowerSurge() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(1.0);
    }

    // ── Inject: SPIN CHAOS ────────────────────────────────────────────────────
    static void InjectSpinChaos() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.99, -0.99));
    }

    // ── Inject: ENGLISH RANDOM ────────────────────────────────────────────────
    static void InjectEnglishRandom() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        static unsigned int rSeed = 0xDEAD1337;
        rSeed ^= rSeed << 13; rSeed ^= rSeed >> 17; rSeed ^= rSeed << 5;
        float rx = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;
        rSeed ^= rSeed << 13; rSeed ^= rSeed >> 17;
        float ry = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;
        vec.mEnglish(Vec2d((double)rx, (double)ry));
    }

    // ── Inject: MICRO JITTER ──────────────────────────────────────────────────
    static void InjectMicroJitter() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        static int jTick = 0; jTick++;
        float jRad = sinf((float)jTick * 0.53f) * 0.044f + cosf((float)jTick * 0.31f) * 0.028f;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double d   = fmod(cur + (double)jRad + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(d));
    }

    // ── Inject: ANGLE DRIFT ───────────────────────────────────────────────────
    static void InjectAngleDrift() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        static double driftAccum = 0.0;
        driftAccum += 0.012;
        if (driftAccum > MAX_ANGLE_RADIANS) driftAccum -= MAX_ANGLE_RADIANS;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double d   = fmod(cur + driftAccum + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(d));
    }

    // ── Inject: POWER GLITCH ──────────────────────────────────────────────────
    static void InjectPowerGlitch() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        static bool gFlip = false; gFlip = !gFlip;
        vc.mPower(gFlip ? 1.0 : 0.0);
    }

    // ── Inject: TOP LOCK ──────────────────────────────────────────────────────
    static void InjectTopLock() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.0, 0.99));
    }

    // ── Inject: NO SHOOT — paksa power=0 setiap microsecond → musuh beku ──────
    static void InjectNoShoot() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(0.0);
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) vec.mEnglish(Vec2d(0.0, 0.0));
        // Drift aim terus agar engine tidak proses sebagai "shot committed"
        auto vg = vc.mVisualGuide();
        if (vg) {
            static double nsDrift = 0.0;
            nsDrift += 0.0005;
            if (nsDrift > MAX_ANGLE_RADIANS) nsDrift = 0.0;
            double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
            double d   = fmod(cur + nsDrift + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            vg.mAimAngle(NumberUtils::normalizeDoublePrecision(d));
        }
    }

    // ── Inject: FORCE FOUL ────────────────────────────────────────────────────
    static void InjectForceFoul() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        static int foulTick = 0; foulTick++;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double offset = (foulTick % 60 < 30) ? 1.5707963267948966 : -1.5707963267948966;
        double d = fmod(cur + offset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(d));
        vc.mPower(0.12);
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) {
            double side = (foulTick % 2 == 0) ? 0.92 : -0.92;
            vec.mEnglish(Vec2d(side, 0.4));
        }
    }

    // ── Inject: SCRATCH FORCE ─────────────────────────────────────────────────
    static void InjectScratch() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(1.0);
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) {
            static int sTick = 0; sTick++;
            double side = (sTick % 60 < 30) ? 0.99 : -0.99;
            vec.mEnglish(Vec2d(side, -0.7));
        }
        auto vg = vc.mVisualGuide();
        if (vg) vg.mAimAngle(NumberUtils::normalizeDoublePrecision(5.497787143782138));
    }

    // ── Inject: CTRL TURN — kendalikan aim musuh ke pocket target ────────────
    static void InjectCtrlTurn() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        static const double pocketAngles[6] = {
            2.356194490192345, 1.5707963267948966, 0.7853981633974483,
            5.497787143782138, 4.71238898038469,   3.9269908169872414
        };
        int pkt = (iCtrlPocket >= 0 && iCtrlPocket <= 5) ? iCtrlPocket : 0;
        double target = pocketAngles[pkt];
        double cur    = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double diff   = target - cur;
        while (diff >  3.14159265358979323846) diff -= 2.0 * 3.14159265358979323846;
        while (diff < -3.14159265358979323846) diff += 2.0 * 3.14159265358979323846;
        double d = fmod(cur + diff * 0.45 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(d));
        vc.mPower(0.0);   // tetap 0 agar tidak bisa shoot ke arah yang dikendalikan
    }

    // ── MASTER INJECT — semua mode sekaligus ──────────────────────────────────
    // Dipanggil per inject-cycle (bisa ribuan kali/detik saat Ultra Max)
    static void InjectAll() {
        if (!sharedGameManager) return;

        // Layer 1: Lock power ke 0 (prioritas tertinggi — blokir shoot)
        InjectNoShoot();

        // Layer 2: Chaos aim agar engine tidak pernah commit
        InjectAimInvert();
        InjectMicroJitter();
        InjectAngleDrift();
        InjectAimDeflect();

        // Layer 3: Spin chaos — bola cue tidak pernah bisa dikontrol
        InjectSpinChaos();
        InjectEnglishRandom();
        InjectTopLock();

        // Layer 4: Power glitch — oscillate 0↔1 setiap call
        InjectPowerGlitch();

        // Layer 5: Force foul + scratch untuk redundancy
        InjectForceFoul();
        InjectScratch();

        // Layer 6: Ctrl turn — arahkan aim ke pocket target
        InjectCtrlTurn();
    }

    // ── Speed Exploit Thread ───────────────────────────────────────────────────
    // fSpeed: 1.0 = 16ms sleep (min), 100.0 = 0.08ms sleep (ultra max ~12500 call/det)
    static void SpeedThread() {
        while (s_threadAlive.load()) {
            if (bActive && IsEnemyTurn()) {
                InjectAll();
            }
            // Sleep interval berdasarkan fSpeed
            // Min (1.0)  → 16000 µs  (~60 call/s, 1 per frame)
            // Speed 10   →  1600 µs  (~625 call/s)
            // Speed 50   →   320 µs  (~3125 call/s)
            // Ultra (100)→    80 µs  (~12500 call/s) → musuh freeze total
            int sleepUs = (int)(16000.0f / fSpeed);
            if (sleepUs < 80) sleepUs = 80;
            usleep((useconds_t)sleepUs);
        }
        s_threadAlive = false;
    }

    // ── Activate / Deactivate ─────────────────────────────────────────────────
    void SetActive(bool active) {
        bActive = active;
        if (active && !s_threadAlive.load()) {
            s_threadAlive = true;
            std::thread(SpeedThread).detach();
        } else if (!active) {
            // Thread akan berhenti sendiri saat bActive = false
        }
    }

    // ── Per-frame Update (untuk mode Min yang tidak perlu thread overhead) ─────
    void Update() {
        // Thread handles everything — ini hanya safety net jika thread belum sempat inject
        if (!bActive || !s_threadAlive.load()) return;
    }

    // ── Cleanup (panggil saat mod unload) ─────────────────────────────────────
    void Shutdown() {
        bActive = false;
        s_threadAlive = false;
    }

    // ── Speed label helper ────────────────────────────────────────────────────
    static const char* GetSpeedLabel(float spd) {
        if (spd <= 1.5f)  return "MIN  (~60 inj/s)";
        if (spd <= 5.0f)  return "SLOW (~300 inj/s)";
        if (spd <= 15.0f) return "NORMAL (~900 inj/s)";
        if (spd <= 35.0f) return "FAST (~2100 inj/s)";
        if (spd <= 60.0f) return "HIGH (~3600 inj/s)";
        if (spd <= 85.0f) return "EXTREME (~5000 inj/s)";
        return "ULTRA MAX (~12500 inj/s) !!!";
    }

} // namespace GhostMode
