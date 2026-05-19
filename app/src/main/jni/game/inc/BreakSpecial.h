#pragma once

// ── Break Special ─────────────────────────────────────────────────────────────
//
// Kombinasi A + B:
//
// [A] Smart Break Score — saat AimBreak, scoring angle tidak hanya hitung bola
//     yang masuk, tapi juga nilai "kesulitan" posisi sisa bola musuh setelah
//     break. Makin dekat ke rail, makin jauh dari pocket = score kesulitan tinggi.
//
// [B] Enemy Nudge — setelah break & klasifikasi diketahui (state 4, bukan giliran
//     break), geser bola-bola musuh sedikit ke arah rail (max 10 unit) supaya
//     posisinya lebih susah di-aim ke pocket.
//
// Cara pakai:
//   - BreakSpecial::ScoreAngleDifficulty() dipanggil dari AimBreak::Aim() sebagai
//     tiebreaker atau secondary score.
//   - BreakSpecial::UpdateNudge() dipanggil tiap frame dari DrawMenu saat aktif.
//
// ─────────────────────────────────────────────────────────────────────────────

#include <cmath>
#include <array>

namespace BreakSpecial {

    // ── Settings ───────────────────────────────────────────────────────────────
    bool bEnabled = false;         // master toggle (dari menu)

    // Nudge: jarak max geser bola musuh ke arah rail (dalam unit meja)
    static constexpr double NUDGE_MAX       = 10.0;
    // Nudge hanya dilakukan 1x per giliran
    static bool  s_nudgeDone     = false;
    static int   s_lastStateId   = -1;

    // ── [A] Score kesulitan posisi bola sisa ───────────────────────────────────
    // Dipanggil dari AimBreak setelah simulasi angle tertentu.
    // Mengembalikan nilai 0.0–1.0: makin tinggi = makin susah posisi bola musuh.
    //
    // Metrik:
    //   1. Jarak minimum ke pocket terdekat — jauh = susah
    //   2. Jarak ke rail meja — dekat rail = susah (cluster near cushion)
    //   3. Jumlah bola masih on-table yang "blocked" (ada bola lain dalam radius)
    double ScoreAngleDifficulty() {
        if (!sharedGameManager) return 0.0;

        // 6 pocket dalam koordinat dunia
        static const Point2D POCKETS[6] = {
            {-TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},
            { 0.0,               TABLE_HALF_HEIGHT},
            { TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},
            { TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},
            { 0.0,              -TABLE_HALF_HEIGHT},
            {-TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},
        };

        double totalDiffScore = 0.0;
        int    counted        = 0;

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.onTable) continue;   // sudah masuk, skip
            if (i == 8) continue;          // 8-ball bukan milik siapa-siapa

            Point2D pos = ball.predictedPosition;

            // 1. Min distance ke pocket — normalisasi ke [0,1]
            double minPktDist = 1e9;
            for (auto& pkt : POCKETS) {
                double dx = pos.x - pkt.x;
                double dy = pos.y - pkt.y;
                double d  = sqrt(dx * dx + dy * dy);
                if (d < minPktDist) minPktDist = d;
            }
            // Pocket terjauh secara diagonal ~155 unit — normalisasi
            double pktScore = std::min(minPktDist / 155.0, 1.0);

            // 2. Distance ke rail terdekat — bola dekat rail = susah di-aim
            double distLeft   = pos.x - TABLE_BOUND_LEFT;
            double distRight  = TABLE_BOUND_RIGHT - pos.x;
            double distTop    = TABLE_BOUND_TOP - pos.y;   // TABLE_BOUND_TOP negatif
            double distBottom = pos.y - TABLE_BOUND_BOTTOM;
            double minRailDist = std::min({std::abs(distLeft), std::abs(distRight),
                                           std::abs(distTop),  std::abs(distBottom)});
            // Rail score: makin dekat rail = nilai makin tinggi; max ~60 unit
            double railScore  = 1.0 - std::min(minRailDist / 60.0, 1.0);

            // 3. Blocked score: ada bola lain dalam radius 3× diameter?
            double blockRadius = BALL_RADIUS * 6.0;
            int    blockers    = 0;
            for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                if (j == i) continue;
                auto& other = gPrediction->guiData.balls[j];
                if (!other.onTable) continue;
                double dx = pos.x - other.predictedPosition.x;
                double dy = pos.y - other.predictedPosition.y;
                if ((dx * dx + dy * dy) <= blockRadius * blockRadius) blockers++;
            }
            double blockScore = std::min(blockers / 3.0, 1.0);

            // Combined — bobot: pktDist 50%, rail 30%, blocked 20%
            double score = pktScore * 0.5 + railScore * 0.3 + blockScore * 0.2;
            totalDiffScore += score;
            counted++;
        }

        return (counted > 0) ? (totalDiffScore / counted) : 0.0;
    }

    // ── [B] Enemy Nudge — geser bola musuh ke arah rail ───────────────────────
    // Dipanggil tiap frame dari DrawMenu.
    // - Hanya aktif saat state 4 (giliran kita) BUKAN di break (sudah ada klasifikasi)
    // - Hanya 1x per giliran (reset saat state berubah)
    void UpdateNudge() {
        if (!bEnabled) return;
        if (!sharedGameManager) return;

        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;
        int stateId = gsm.getCurrentStateId();

        // Reset flag saat state berubah (giliran baru)
        if (stateId != s_lastStateId) {
            if (s_nudgeDone && stateId == 4) s_nudgeDone = false;
            s_lastStateId = stateId;
        }

        // Hanya aktif saat giliran kita & belum nudge di giliran ini
        if (stateId != 4) return;
        if (s_nudgeDone)   return;

        // Perlu mengetahui klasifikasi pemain — hanya setelah break selesai
        BallEnums::Classification myClass = sharedGameManager.getPlayerClassification();
        if (myClass != BallEnums::Classification::SOLID &&
            myClass != BallEnums::Classification::STRIPE) return; // masih di break, skip

        BallEnums::Classification enemyClass =
            (myClass == BallEnums::Classification::SOLID)
                ? BallEnums::Classification::STRIPE
                : BallEnums::Classification::SOLID;

        Table table = sharedGameManager.mTable;
        if (!table) return;
        auto& balls = table.mBalls();
        if (!balls) return;

        LOGI("BreakSpecial: nudging enemy balls (enemy class=%d)", (int)enemyClass);

        for (int i = 1; i < balls.Count; i++) {
            Ball ball = balls[i];
            if (!ball) continue;
            if (ball.classification() != enemyClass) continue;
            if (!ball.isOnTable()) continue;

            Vec2d pos = ball.position();

            // Hitung arah ke rail terdekat
            double distLeft   = pos.x - TABLE_BOUND_LEFT;
            double distRight  = TABLE_BOUND_RIGHT  - pos.x;
            double distTop    = std::abs(TABLE_BOUND_TOP - pos.y);   // top = negatif
            double distBottom = pos.y - TABLE_BOUND_BOTTOM;

            double nudgeX = 0.0, nudgeY = 0.0;

            // Pilih rail terdekat sebagai arah nudge
            double minDist = std::min({distLeft, distRight, distTop, distBottom});
            if (minDist == distLeft) {
                nudgeX = -NUDGE_MAX;
            } else if (minDist == distRight) {
                nudgeX =  NUDGE_MAX;
            } else if (minDist == distTop) {
                nudgeY = -NUDGE_MAX;   // TABLE_BOUND_TOP negatif = ke atas
            } else {
                nudgeY =  NUDGE_MAX;
            }

            // Terapkan nudge — clamp dalam batas meja
            Vec2d newPos;
            newPos.x = std::max(TABLE_BOUND_LEFT,   std::min(TABLE_BOUND_RIGHT,  pos.x + nudgeX));
            newPos.y = std::max(TABLE_BOUND_BOTTOM,  std::min(-TABLE_BOUND_TOP,   pos.y + nudgeY));

            // Pastikan tidak overlap dengan bola lain (jarak min = 2 * radius)
            bool overlap = false;
            for (int j = 0; j < balls.Count; j++) {
                if (j == i) continue;
                Ball other = balls[j];
                if (!other || !other.isOnTable()) continue;
                Vec2d op  = other.position();
                double dx = newPos.x - op.x;
                double dy = newPos.y - op.y;
                double minDist2 = BALL_RADIUS * 2.0 + 0.5;
                if ((dx * dx + dy * dy) < minDist2 * minDist2) {
                    overlap = true;
                    break;
                }
            }

            if (!overlap) {
                ball.position(newPos);
                LOGI("BreakSpecial: ball[%d] nudged (%.1f,%.1f) -> (%.1f,%.1f)",
                     i, pos.x, pos.y, newPos.x, newPos.y);
            }
        }

        s_nudgeDone = true;
    }

} // namespace BreakSpecial
