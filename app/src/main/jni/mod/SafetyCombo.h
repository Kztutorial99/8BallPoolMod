#pragma once

#include "game/inc/NumberUtils.h"
#include "game/inc/Prediction.fast.h"
#include "game/GameManager.h"

// ── SafetyCombo.h ─────────────────────────────────────────────────────────────
// Angle-scan helpers:
//  SafetyFinder::Find()  — cue ball furthest from enemy after shot (pure safety)
//  ComboFinder::Find()   — pots the most balls of our class in one shot
//  HeatmapESP::Compute() — scans angles, caches which ones produce any pot
//  HeatmapESP::Draw()    — renders cached dots around cue ball on screen
// ─────────────────────────────────────────────────────────────────────────────

static inline double sc_normalizeAngle(double angle) {
    const double full = 2.0 * M_PI;
    angle = fmod(angle, full);
    if (angle < 0.0) angle += full;
    return angle;
}

static inline void sc_setAimAngle(double angle) {
    sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
}

// ── Safety Finder ─────────────────────────────────────────────────────────────
// Scans all angles to find the shot where the cue ball lands FURTHEST from
// enemy balls after the shot, without potting any of our own balls.
namespace SafetyFinder {

    static void Find(double step = 0.15) {
        if (!sharedGameManager) return;
        if (!gPrediction) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        Ball::Classification enemyClass =
            (myclass == Ball::Classification::SOLID)
            ? Ball::Classification::STRIPE : Ball::Classification::SOLID;

        double best_dist  = -1.0;
        double base = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());
        double best_angle = base;

        // FIX: Use count-based loop instead of float equality comparison.
        // a != base was an infinite loop due to floating-point accumulation.
        const int maxIter = (int)((2.0 * M_PI) / fabs(step)) + 2;
        for (int iter = 0; iter < maxIter; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                sc_normalizeAngle(base + step * (double)(iter + 1)));

            gPrediction->determineShotResult(true, a);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;

            bool potted = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.originalOnTable && !b.onTable) { potted = true; break; }
            }
            if (potted) continue;

            double min_d = 1e9;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (!b.originalOnTable || b.classification != enemyClass) continue;
                double dx = cue.predictedPosition.x - b.initialPosition.x;
                double dy = cue.predictedPosition.y - b.initialPosition.y;
                double d  = sqrt(dx*dx + dy*dy);
                if (d < min_d) min_d = d;
            }
            if (min_d > best_dist) { best_dist = min_d; best_angle = a; }
        }
        sc_setAimAngle(best_angle);
    }

} // namespace SafetyFinder

// ── Combo Finder ──────────────────────────────────────────────────────────────
// Scans all angles to find the shot that pots the most balls of our class.
namespace ComboFinder {

    static void Find(double step = 0.05) {
        if (!sharedGameManager) return;
        if (!gPrediction) return;
        if (sharedGameManager.is9BallGame()) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        bool only8B = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable) { only8B = false; break; }
        }
        Ball::Classification target = only8B ? Ball::Classification::EIGHT_BALL : myclass;

        int    best_score = -1;
        double base = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());
        double best_angle = base;

        // FIX: Use count-based loop instead of float equality comparison.
        // a != base was an infinite loop due to floating-point accumulation.
        const int maxIter = (int)((2.0 * M_PI) / fabs(step)) + 2;
        for (int iter = 0; iter < maxIter; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                sc_normalizeAngle(base + step * (double)(iter + 1)));

            gPrediction->determineShotResult(true, a);
            auto& cue = gPrediction->guiData.balls[0];
            auto& b8  = gPrediction->guiData.balls[8];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (gPrediction->guiData.collision.firstHitBall->classification != target) continue;
            if (!only8B && b8.originalOnTable && !b8.onTable) continue;

            int score = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.classification == target && b.originalOnTable && !b.onTable) score++;
            }
            if (score > best_score) { best_score = score; best_angle = a; }
        }
        sc_setAimAngle(best_angle);
    }

} // namespace ComboFinder

// ── Heatmap ESP ───────────────────────────────────────────────────────────────
// Computes scoring dots around cue ball once per player turn.
namespace HeatmapESP {

    struct HeatEntry { double angle; int potCount; };
    static constexpr int MAX_E = 256;
    static HeatEntry g_entries[MAX_E];
    static int       g_count     = 0;
    static bool      g_dirty     = true;
    static int       g_prevState = -1;

    static void Compute(int stateId) {
        if (stateId != 4) {
            if (g_prevState == 4) g_dirty = true;
            g_prevState = stateId;
            return;
        }
        g_prevState = stateId;
        if (!g_dirty) return;
        g_count = 0;
        g_dirty = false;

        if (!sharedGameManager) return;
        if (!gPrediction) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        bool is9 = sharedGameManager.is9BallGame();
        Ball::Classification tgt = myclass;
        if (!is9) {
            bool only8 = true;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.classification == myclass && b.originalOnTable) { only8 = false; break; }
            }
            if (only8) tgt = Ball::Classification::EIGHT_BALL;
        }

        constexpr double STEP  = 5.0 * M_PI / 180.0;
        const int totalSteps = (int)(2.0 * M_PI / STEP) + 1;

        for (int i = 0; i < totalSteps && g_count < MAX_E; i++) {
            double a  = (double)i * STEP;
            double na = NumberUtils::normalizeDoublePrecision(sc_normalizeAngle(a));
            gPrediction->determineShotResult(true, na);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (!is9 && gPrediction->guiData.collision.firstHitBall->classification != tgt) continue;

            auto& b8 = gPrediction->guiData.balls[8];
            if (!is9 && tgt != Ball::Classification::EIGHT_BALL &&
                b8.originalOnTable && !b8.onTable) continue;

            int pots = 0;
            for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                auto& b = gPrediction->guiData.balls[j];
                if (b.originalOnTable && !b.onTable) pots++;
            }
            if (pots > 0) g_entries[g_count++] = { na, pots };
        }

        // Restore prediction to current game angle/power
        gPrediction->determineShotResult(false);
    }

    // cueScreenPos should be precomputed with WorldToScreen before calling
    static void Draw(ImDrawList* draw, ImVec2 cueScreenPos) {
        for (int i = 0; i < g_count; i++) {
            auto& e  = g_entries[i];
            float fx = cueScreenPos.x + (float)cos(e.angle) * 72.0f;
            float fy = cueScreenPos.y + (float)sin(e.angle) * 72.0f;

            ImU32 col = (e.potCount >= 3) ? IM_COL32(255, 50,  50,  210)
                      : (e.potCount == 2) ? IM_COL32(255, 200, 0,   210)
                                          : IM_COL32(0,   220, 90,  190);
            draw->AddCircleFilled(ImVec2(fx, fy), 7.0f, col);
            draw->AddCircle(ImVec2(fx, fy), 8.0f, IM_COL32(0, 0, 0, 100), 0, 1.2f);
        }
    }

} // namespace HeatmapESP
