#pragma once

// ── Enemy Sabotage — Runtime Memory Manipulation During Opponent's Turn ───────
// Modifies game state variables directly in memory while state == 7/8 (enemy turn)
//
// 19 Modes:
//   0  Aim Deflect    — fixed angle offset on enemy aim angle
//   1  Power Drain    — forces mPower to near-zero (too weak to reach pocket)
//   2  Spin Chaos     — injects extreme back+side english (wildly erratic)
//   3  Combo          — Deflect + Drain + Spin simultaneously
//   4  Aim Invert     — deflects aim by π rad (shoots exact opposite direction)
//   5  Micro Jitter   — rapid oscillating small deflect (hard to detect/correct)
//   6  Top Lock       — pure max topspin (cue ball rockets uncontrollably forward)
//   7  Power Surge    — forces mPower to 1.0 (overshoot → scratch / miss)
//   8  Chaos Extreme  — Invert + Surge + extreme spin all at once
//   9  Angle Zero     — lock aim angle to 0.0 (shoots straight into rail)
//  10  Eng Random     — inject pseudo-random english XY every frame
//  11  Pwr Glitch     — oscillate power 0↔1 every tick (engine can't lock)
//  12  Angle Drift    — accumulating offset per frame (gets worse over time)
//  13  Total Blackout — ALL sabotages at max simultaneously (ultimate risk)
// ── ULTRA RISK ─────────────────────────────────────────────────────────────────
//  14  No Shoot       — force power=0 every frame → enemy literally cannot shoot
//  15  Ctrl Turn      — hijack enemy aim to our desired pocket every frame
//  16  Force Foul     — redirect aim 90° off so enemy always fouls/misses
//  17  Scratch+       — max power + extreme double-side english → guaranteed scratch
//  18  GODMODE        — No Shoot + Ctrl Turn + Total Blackout all at once
// ─────────────────────────────────────────────────────────────────────────────

namespace EnemySabotage {

    bool  bActive      = false;
    int   iMode        = 0;
    float fOffset      = 0.09f;   // aim deflect offset in radians
    // Ctrl Turn: pocket index to force (0-5), -1 = auto (rail redirect)
    int   iCtrlPocket  = 0;

    static bool IsEnemyTurn() {
        if (!sharedGameManager) return false;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return false;
        int sid = gsm.getCurrentStateId();
        return sid == 7 || sid == 8;
    }

    // ── Mode 0: Aim Deflect ───────────────────────────────────────────────────
    static void DoAimDeflect() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + (double)fOffset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    // ── Mode 1: Power Drain ───────────────────────────────────────────────────
    static void DoPowerDrain() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(0.012);
    }

    // ── Mode 2: Spin Chaos ────────────────────────────────────────────────────
    static void DoSpinChaos() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.95, -0.95));
    }

    // ── Mode 4: Aim Invert ────────────────────────────────────────────────────
    static void DoAimInvert() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double inverted = fmod(cur + 3.14159265358979323846 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(inverted));
    }

    // ── Mode 5: Micro Jitter ──────────────────────────────────────────────────
    static void DoMicroJitter() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        static int jTick = 0;
        jTick++;
        float jRad = sinf((float)jTick * 0.43f) * 0.036f
                   + cosf((float)jTick * 0.27f) * 0.022f;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double deflected = fmod(cur + (double)jRad + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(deflected));
    }

    // ── Mode 6: Top Lock ──────────────────────────────────────────────────────
    static void DoTopLock() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.0, 0.99));
    }

    // ── Mode 7: Power Surge ───────────────────────────────────────────────────
    static void DoPowerSurge() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(1.0);
    }

    // ── Mode 8: Chaos Extreme ─────────────────────────────────────────────────
    static void DoChaosExtreme() {
        DoAimInvert();
        DoPowerSurge();
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        vec.mEnglish(Vec2d(0.99, 0.99));
    }

    // ── Mode 9: Angle Zero ────────────────────────────────────────────────────
    static void DoAngleZero() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(0.0));
    }

    // ── Mode 10: English Random ───────────────────────────────────────────────
    static void DoEnglishRandom() {
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        static unsigned int rSeed = 0x1337BEEF;
        rSeed ^= rSeed << 13;
        rSeed ^= rSeed >> 17;
        rSeed ^= rSeed << 5;
        float rx = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;
        rSeed ^= rSeed << 13;
        rSeed ^= rSeed >> 17;
        float ry = ((float)(rSeed & 0xFFFF) / 32767.5f) - 1.0f;
        vec.mEnglish(Vec2d((double)rx, (double)ry));
    }

    // ── Mode 11: Power Glitch ─────────────────────────────────────────────────
    static void DoPowerGlitch() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        static bool glitchFlip = false;
        glitchFlip = !glitchFlip;
        vc.mPower(glitchFlip ? 1.0 : 0.005);
    }

    // ── Mode 12: Angle Drift ──────────────────────────────────────────────────
    static void DoAngleDrift() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        static double driftAccum = 0.0;
        driftAccum += 0.008;
        if (driftAccum > MAX_ANGLE_RADIANS) driftAccum -= MAX_ANGLE_RADIANS;
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double drifted = fmod(cur + driftAccum + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(drifted));
    }

    // ── Mode 13: Total Blackout ───────────────────────────────────────────────
    static void DoTotalBlackout() {
        DoAngleZero();
        DoPowerSurge();
        DoSpinChaos();
        DoEnglishRandom();
        static bool bbFlip = false;
        bbFlip = !bbFlip;
        if (bbFlip) DoPowerDrain();
    }

    // ══════════ ULTRA RISK MODES ══════════════════════════════════════════════

    // ── Mode 14: No Shoot — musuh TIDAK BISA shoot sama sekali ───────────────
    // Force power ke 0 setiap frame + clear english → shot tidak pernah bisa
    // terjadi karena power selalu direset sebelum engine bisa proses
    static void DoNoShoot() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        // Lock power ke nilai yang terlalu kecil untuk men-trigger shot
        vc.mPower(0.0);
        // Clear english juga agar tidak ada efek samping
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) vec.mEnglish(Vec2d(0.0, 0.0));
        // Reset aim ke posisi safe agar GUI tidak crash
        auto vg = vc.mVisualGuide();
        if (vg) {
            double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
            // Tambah offset kecil setiap frame agar aim terus bergerak
            // sehingga engine tidak mengira shot sudah final
            static double noShootDrift = 0.0;
            noShootDrift += 0.001;
            if (noShootDrift > MAX_ANGLE_RADIANS) noShootDrift = 0.0;
            double newAngle = fmod(cur + noShootDrift + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            vg.mAimAngle(NumberUtils::normalizeDoublePrecision(newAngle));
        }
    }

    // ── Mode 15: Ctrl Turn — kendalikan giliran musuh ─────────────────────────
    // Hijack aim musuh ke arah yang kita tentukan setiap frame
    // Musuh "shooting" tapi bolanya pergi ke mana kita arahkan
    static void DoCtrlTurn() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;

        // Hitung arah ke pocket yang diinginkan berdasarkan iCtrlPocket
        // Pocket angles (approx, dari tengah meja):
        // 0=top-left(2.36), 1=top-center(1.57), 2=top-right(0.79)
        // 3=bot-right(5.50), 4=bot-center(4.71), 5=bot-left(3.93)
        static const double pocketAngles[6] = {
            2.356194490192345,   // top-left  (3π/4)
            1.5707963267948966,  // top-center (π/2)
            0.7853981633974483,  // top-right  (π/4)
            5.497787143782138,   // bot-right  (7π/4)
            4.71238898038469,    // bot-center (3π/2)
            3.9269908169872414   // bot-left   (5π/4)
        };

        int pkt = iCtrlPocket;
        if (pkt < 0 || pkt > 5) pkt = 0;

        double targetAngle = pocketAngles[pkt];
        // Smooth lerp ke target agar tidak terlalu obvious
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        double diff = targetAngle - cur;
        // Normalize diff ke -π..π
        while (diff >  3.14159265358979323846) diff -= 2.0 * 3.14159265358979323846;
        while (diff < -3.14159265358979323846) diff += 2.0 * 3.14159265358979323846;
        double newAngle = fmod(cur + diff * 0.35 + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(newAngle));

        // Set power sedang agar shot terjadi tapi ke arah kita
        vc.mPower(0.55);
    }

    // ── Mode 16: Force Foul — paksa musuh foul setiap giliran ────────────────
    // Redirect aim 90° ± random dari posisi saat ini → selalu miss / foul
    // Kombinasi dengan power rendah agar bola tidak reach pocket sama sekali
    static void DoForceFoul() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;

        // Offset tepat 90° (π/2) dari aim saat ini → selalu miss
        double cur = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        static int foulTick = 0;
        foulTick++;
        // Alternating +90° / -90° agar musuh tidak bisa kompensasi
        double foulOffset = (foulTick % 60 < 30)
            ?  1.5707963267948966   // +90°
            : -1.5707963267948966;  // -90°
        double newAngle = fmod(cur + foulOffset + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        vg.mAimAngle(NumberUtils::normalizeDoublePrecision(newAngle));

        // Power rendah agar tetap foul bukan sekedar meleset
        vc.mPower(0.18);

        // English ekstrim ke samping untuk double-foul
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) {
            double side = (foulTick % 2 == 0) ? 0.88 : -0.88;
            vec.mEnglish(Vec2d(side, 0.3));
        }
    }

    // ── Mode 17: Scratch+ — paksa musuh scratch setiap giliran ───────────────
    // Max power + double english ekstrim ke arah pocket terdekat
    // → cue ball masuk pocket = scratch = giliran balik + ball-in-hand
    static void DoScratchPlus() {
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;

        // Max power → bola cue meluncur sangat kencang
        vc.mPower(1.0);

        // English: max side spin + back spin → cue ball melengkung ke pocket
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (vec) {
            static int scratchTick = 0;
            scratchTick++;
            // Alternating left/right extreme spin setiap 30 frame
            double side = (scratchTick % 60 < 30) ? 0.99 : -0.99;
            vec.mEnglish(Vec2d(side, -0.6));   // side + backspin
        }

        // Aim ke sudut yang dekat pocket cue ball agar scratch terjadi
        auto vg = vc.mVisualGuide();
        if (vg) {
            static double scratchAngle = 5.497787143782138; // bot-right pocket
            vg.mAimAngle(NumberUtils::normalizeDoublePrecision(scratchAngle));
        }
    }

    // ── Mode 18: GODMODE — gabungan No Shoot + Ctrl Turn + Total Blackout ────
    // Mode paling ekstrim: power dikunci 0, aim dikendalikan, semua sabotase aktif
    // SANGAT BERESIKO — kemungkinan crash / kick dari game tinggi
    static void DoGodMode() {
        // Frame counter untuk alternating effects
        static int godTick = 0;
        godTick++;

        if (godTick % 3 == 0) {
            DoNoShoot();         // frame 0,3,6... → power dikunci 0
        } else if (godTick % 3 == 1) {
            DoCtrlTurn();        // frame 1,4,7... → aim dikendalikan
        } else {
            DoTotalBlackout();   // frame 2,5,8... → full blackout
        }

        // Selalu jalankan scratch untuk memastikan apapun yang lolos = scratch
        DoScratchPlus();
    }

    // ─────────────────────────────────────────────────────────────────────────
    void Update() {
        if (!bActive || !sharedGameManager) return;
        if (!IsEnemyTurn()) return;

        switch (iMode) {
            case 0:  DoAimDeflect();                                    break;
            case 1:  DoPowerDrain();                                    break;
            case 2:  DoSpinChaos();                                     break;
            case 3:  DoAimDeflect(); DoPowerDrain(); DoSpinChaos();     break;
            case 4:  DoAimInvert();                                     break;
            case 5:  DoMicroJitter();                                   break;
            case 6:  DoTopLock();                                       break;
            case 7:  DoPowerSurge();                                    break;
            case 8:  DoChaosExtreme();                                  break;
            case 9:  DoAngleZero();                                     break;
            case 10: DoEnglishRandom();                                 break;
            case 11: DoPowerGlitch();                                   break;
            case 12: DoAngleDrift();                                    break;
            case 13: DoTotalBlackout();                                 break;
            case 14: DoNoShoot();                                       break;
            case 15: DoCtrlTurn();                                      break;
            case 16: DoForceFoul();                                     break;
            case 17: DoScratchPlus();                                   break;
            case 18: DoGodMode();                                       break;
            default: break;
        }
    }

} // namespace EnemySabotage
