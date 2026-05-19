#pragma once

// ── Auto Power Control ────────────────────────────────────────────────────────
// Menghitung dan menetapkan shot power optimal secara otomatis berdasarkan:
//   - Jarak cue ball → target ball
//   - Jarak target ball → pocket
//   - Tipe tembakan (break = max, dekat = rendah, jauh = tinggi)
// Menulis langsung ke VisualCue::mPower (normalized 0.0 – 1.0)
// ─────────────────────────────────────────────────────────────────────────────

namespace AutoPower {

    // Pocket world positions (sesuai dump)
    static constexpr double POCKET_X[6] = {-127.0,   0.0, 127.0,  127.0,  0.0, -127.0};
    static constexpr double POCKET_Y[6] = {  63.5,  63.5,  63.5,  -63.5, -63.5,  -63.5};

    static inline double Dist2D(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return sqrt(dx * dx + dy * dy);
    }

    // Tembakan normal: hitung jarak total dan map ke power
    void Apply(int targetBallIdx, int targetPocketIdx) {
        if (!sharedGameManager || !gPrediction) return;

        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;

        if (gPrediction->guiData.ballsCount <= 0) return;

        auto& cueBall = gPrediction->guiData.balls[0];

        double power = 0.60; // default

        if (targetBallIdx >= 0 && targetBallIdx < gPrediction->guiData.ballsCount) {
            auto& targetBall = gPrediction->guiData.balls[targetBallIdx];

            // Jarak cue → target
            double d1 = Dist2D(
                cueBall.initialPosition.x, cueBall.initialPosition.y,
                targetBall.initialPosition.x, targetBall.initialPosition.y);

            // Jarak target → pocket
            double d2 = 0.0;
            if (targetPocketIdx >= 0 && targetPocketIdx < 6) {
                d2 = Dist2D(
                    targetBall.initialPosition.x, targetBall.initialPosition.y,
                    POCKET_X[targetPocketIdx], POCKET_Y[targetPocketIdx]);
            }

            double totalDist = d1 + d2;

            // Jarak meja maksimum diagonal ~ 283 units
            // Map ke range power 0.22 – 0.88
            constexpr double MAX_DIST = 330.0;
            power = 0.22 + (totalDist / MAX_DIST) * 0.66;
            if (power > 0.88) power = 0.88;
            if (power < 0.22) power = 0.22;
        }

        vc.mPower(power);
    }

    // Break shot: selalu power penuh
    void ApplyBreak() {
        if (!sharedGameManager) return;
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        vc.mPower(1.0);
    }

    // 9-ball shot: sedikit lebih keras dari normal (bola banyak di meja)
    void Apply9Ball(int targetBallIdx, int targetPocketIdx) {
        if (!sharedGameManager || !gPrediction) return;
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;

        if (gPrediction->guiData.ballsCount <= 0) return;
        auto& cueBall = gPrediction->guiData.balls[0];

        double power = 0.65;

        if (targetBallIdx >= 0 && targetBallIdx < gPrediction->guiData.ballsCount) {
            auto& targetBall = gPrediction->guiData.balls[targetBallIdx];
            double d1 = Dist2D(
                cueBall.initialPosition.x, cueBall.initialPosition.y,
                targetBall.initialPosition.x, targetBall.initialPosition.y);
            double d2 = 0.0;
            if (targetPocketIdx >= 0 && targetPocketIdx < 6) {
                d2 = Dist2D(
                    targetBall.initialPosition.x, targetBall.initialPosition.y,
                    POCKET_X[targetPocketIdx], POCKET_Y[targetPocketIdx]);
            }
            double totalDist = d1 + d2;
            constexpr double MAX_DIST = 330.0;
            power = 0.28 + (totalDist / MAX_DIST) * 0.62;
            if (power > 0.90) power = 0.90;
            if (power < 0.28) power = 0.28;
        }

        vc.mPower(power);
    }

} // namespace AutoPower
