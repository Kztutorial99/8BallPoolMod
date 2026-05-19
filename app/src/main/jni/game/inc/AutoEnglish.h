#pragma once

// ── Auto English / Spin Control ───────────────────────────────────────────────
// Menetapkan spin optimal pada cue ball secara otomatis berdasarkan:
//   - Jarak tembakan (dekat = back spin, jauh = top spin)
//   - Posisi cue ball terhadap tepi meja (cegah scratch)
//   - Tipe tembakan (break = top, safety = back)
// Menulis langsung ke VisualEnglishControl::mEnglish (Vec2d, range -1.0 ~ 1.0)
//   x = spin kiri/kanan (negatif=kiri, positif=kanan)
//   y = spin atas/bawah (negatif=back/stop, positif=top/follow)
// ─────────────────────────────────────────────────────────────────────────────

namespace AutoEnglish {

    static constexpr double ZONE_NEAR   = 40.0;
    static constexpr double ZONE_MEDIUM = 110.0;

    static constexpr double CUSHION_DANGER = 18.0;  // jarak dari tepi dianggap berbahaya

    static inline double Dist2D(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return sqrt(dx * dx + dy * dy);
    }

    static inline void WriteEnglish(double spinX, double spinY) {
        if (!sharedGameManager) return;
        auto vec = sharedGameManager.mVisualEnglishControl();
        if (!vec) return;
        // Clamp ke -1.0 ~ 1.0
        if (spinX >  1.0) spinX =  1.0;
        if (spinX < -1.0) spinX = -1.0;
        if (spinY >  1.0) spinY =  1.0;
        if (spinY < -1.0) spinY = -1.0;
        vec.mEnglish(Vec2d(spinX, spinY));
    }

    // Cek apakah posisi terlalu dekat dengan tepi meja (risiko scratch)
    static bool IsNearCushion(double x, double y) {
        return (x < -TABLE_HALF_WIDTH  + CUSHION_DANGER ||
                x >  TABLE_HALF_WIDTH  - CUSHION_DANGER ||
                y < -TABLE_HALF_HEIGHT + CUSHION_DANGER ||
                y >  TABLE_HALF_HEIGHT - CUSHION_DANGER);
    }

    // Tembakan normal (8-ball predict / 9-ball predict)
    void Apply(int targetBallIdx) {
        if (!sharedGameManager || !gPrediction) return;
        if (gPrediction->guiData.ballsCount <= 0) return;

        auto& cueBall    = gPrediction->guiData.balls[0];
        double cx        = cueBall.initialPosition.x;
        double cy        = cueBall.initialPosition.y;

        double spinX = 0.0;
        double spinY = 0.0;

        if (targetBallIdx >= 0 && targetBallIdx < gPrediction->guiData.ballsCount) {
            auto& target = gPrediction->guiData.balls[targetBallIdx];
            double dist = Dist2D(cx, cy,
                target.initialPosition.x, target.initialPosition.y);

            if (dist < ZONE_NEAR) {
                // Tembakan dekat → back spin agar cue ball berhenti / tidak lanjut
                spinY = -0.45;
            } else if (dist < ZONE_MEDIUM) {
                // Tembakan sedang → center (natural roll)
                spinY = 0.0;
            } else {
                // Tembakan jauh → slight top spin agar cue ball tetap bergerak
                spinY = 0.28;
            }
        }

        // Koreksi posisi cue ball: jika terlalu dekat tepi, tambahkan back spin
        // untuk mengurangi risiko scratch ke pocket sudut
        if (IsNearCushion(cx, cy)) {
            spinY -= 0.20;
            if (spinY < -0.80) spinY = -0.80;
        }

        WriteEnglish(spinX, spinY);
    }

    // Break shot: top spin penuh untuk penetrasi maksimal ke rack bola
    void ApplyBreak() {
        WriteEnglish(0.0, 0.70);
    }

    // Aim Lock 8 Ball: back spin ringan agar cue ball tidak masuk setelah bola 8 masuk
    void Apply8Lock() {
        if (!sharedGameManager || !gPrediction) return;
        if (gPrediction->guiData.ballsCount <= 0) return;

        auto& cueBall = gPrediction->guiData.balls[0];
        double cx = cueBall.initialPosition.x;
        double cy = cueBall.initialPosition.y;

        // Cek jarak ke bola 8
        double dist = 50.0;
        if (8 < gPrediction->guiData.ballsCount) {
            auto& eightBall = gPrediction->guiData.balls[8];
            dist = Dist2D(cx, cy,
                eightBall.initialPosition.x, eightBall.initialPosition.y);
        }

        double spinY = (dist < ZONE_NEAR) ? -0.55 : -0.30;
        double spinX = 0.0;

        if (IsNearCushion(cx, cy)) spinY -= 0.15;
        if (spinY < -1.0) spinY = -1.0;

        WriteEnglish(spinX, spinY);
    }

    // Reset ke center spin (tanpa spin)
    void Reset() {
        WriteEnglish(0.0, 0.0);
    }

} // namespace AutoEnglish
