#pragma once

// Prediction.fast.h is already included before this file via game.h
// Do NOT re-include Prediction.h to avoid redefinition errors
#include <imgui/imgui.h>

using namespace ImGui;

namespace AutoAim {
    double lastSetAngle = 0.0;

    static inline double normalizeAngle(double angle) {
        if (angle >= MAX_ANGLE_RADIANS) angle = fmod(angle, MAX_ANGLE_RADIANS);
        else if (angle < 0.0) angle = MAX_ANGLE_RADIANS - fmod(-angle, MAX_ANGLE_RADIANS);
        return angle;
    }

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    // Always use max power so prediction matches full-power shot
    static double aimPower() {
        return CUE_PROPERTIES_MAX_POWER;
    }

    // Apply best spin to maximize pot chance (uses VisualEnglishControl)
    static void ApplyBestSpin(double angle) {
        double pwr = aimPower();
        Vec2d curSpin = sharedGameManager.getShotSpin();

        static const double spinR[] = {0.0, 0.35, 0.7, 1.0};
        static const double spinAngles[] = {0.0, M_PI/4, M_PI/2, 3*M_PI/4,
                                             M_PI, 5*M_PI/4, 3*M_PI/2, 7*M_PI/4};
        double maxSpin = CUE_PROPERTIES_SPIN;

        double bestScore = -1e18;
        double bestSX = 0.0, bestSY = 0.0;
        bool found = false;

        for (double r : spinR) {
            for (double sa : spinAngles) {
                double sx = cos(sa) * r * maxSpin;
                double sy = sin(sa) * r * maxSpin;
                Vec2d testSpin(sx, sy);

                gPrediction->determineShotResult(true, angle, pwr, testSpin);

                auto& cueBall = gPrediction->guiData.balls[0];
                if (!cueBall.onTable) continue;

                double score = 0.0;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (b.originalOnTable && !b.onTable) score += 100.0;
                }
                if (score > bestScore) {
                    bestScore = score; bestSX = sx; bestSY = sy; found = true;
                }
            }
        }
        if (found) {
            sharedGameManager.mVisualEnglishControl().mEnglish(Vec2d(bestSX, bestSY));
        }
    }

    // 8-Ball mode: multi-pass coarse→fine scan
    void AIM(double coarseStep = 0.08, double fineStep = 0.015) {
        double startAngle = sharedGameManager.mVisualCue().getShotAngle();
        double pwr = aimPower();

        Ball::Classification myclass = (Ball::Classification)sharedGameManager.getPlayerClassification();

        // Determine if only 8-ball is left for player
        bool only8Left = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable) { only8Left = false; break; }
        }
        Ball::Classification target = only8Left ? Ball::Classification::EIGHT_BALL : myclass;

        // Find 8-ball index
        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        Vec2d spin = sharedGameManager.getShotSpin();

        struct Cand { double angle; int cnt; };
        std::vector<Cand> coarseCands;
        int coarseN = (int)(MAX_ANGLE_RADIANS / coarseStep) + 2;

        for (int ci = 0; ci < coarseN; ci++) {
            double angle = normalizeAngle(startAngle + ci * coarseStep);
            gPrediction->determineShotResult(true, angle, pwr, spin);

            auto& cueBall = gPrediction->guiData.balls[0];
            if (cueBall.originalOnTable && !cueBall.onTable) continue;
            if (!only8Left && eightBallIdx >= 0) {
                auto& eb = gPrediction->guiData.balls[eightBallIdx];
                if (eb.originalOnTable && !eb.onTable) continue;
            }

            bool good = false; int cnt = 0;
            if (gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->classification == target) {
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (b.classification == target && b.originalOnTable && !b.onTable) {
                        good = true; cnt++;
                    }
                }
            }
            if (good) coarseCands.push_back({angle, cnt});
        }

        if (!coarseCands.empty()) {
            auto best = *std::max_element(coarseCands.begin(), coarseCands.end(),
                [](const Cand& a, const Cand& b){ return a.cnt < b.cnt; });

            double fineStart = best.angle - coarseStep;
            Cand bestFine = best;
            int fineN = (int)(coarseStep * 2.0 / fineStep) + 4;

            for (int fi = 0; fi < fineN; fi++) {
                double angle = normalizeAngle(fineStart + fi * fineStep);
                gPrediction->determineShotResult(true, angle, pwr, spin);

                auto& cueBall = gPrediction->guiData.balls[0];
                if (cueBall.originalOnTable && !cueBall.onTable) continue;
                if (!only8Left && eightBallIdx >= 0) {
                    auto& eb = gPrediction->guiData.balls[eightBallIdx];
                    if (eb.originalOnTable && !eb.onTable) continue;
                }

                bool good = false; int cnt = 0;
                if (gPrediction->guiData.collision.firstHitBall &&
                    gPrediction->guiData.collision.firstHitBall->classification == target) {
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        auto& b = gPrediction->guiData.balls[i];
                        if (b.classification == target && b.originalOnTable && !b.onTable) {
                            good = true; cnt++;
                        }
                    }
                }
                if (good && cnt >= bestFine.cnt) bestFine = {angle, cnt};
            }
            setAimAngle(bestFine.angle);
            ApplyBestSpin(bestFine.angle);
            return;
        }

        // Fallback: full fine scan
        int fullN = (int)(MAX_ANGLE_RADIANS / fineStep) + 2;
        for (int fi = 0; fi < fullN; fi++) {
            double angle = normalizeAngle(startAngle + fi * fineStep);
            gPrediction->determineShotResult(true, angle, pwr, spin);

            auto& cueBall = gPrediction->guiData.balls[0];
            if (cueBall.originalOnTable && !cueBall.onTable) continue;
            if (!only8Left && eightBallIdx >= 0) {
                auto& eb = gPrediction->guiData.balls[eightBallIdx];
                if (eb.originalOnTable && !eb.onTable) continue;
            }

            bool good = false;
            if (gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->classification == target) {
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (b.classification == target && b.originalOnTable && !b.onTable) { good = true; break; }
                }
            }
            if (good) { setAimAngle(angle); ApplyBestSpin(angle); return; }
        }
    }

    // 9-Ball mode: aim at lowest-numbered ball on table
    void AIM_9Ball(double coarseStep = 0.08, double fineStep = 0.015) {
        double startAngle = sharedGameManager.mVisualCue().getShotAngle();
        double pwr = aimPower();
        Vec2d spin = sharedGameManager.getShotSpin();

        // Find lowest-numbered ball on table (index 1–9)
        int targetIdx = -1;
        for (int i = 1; i <= 9 && i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].originalOnTable) { targetIdx = i; break; }
        }
        if (targetIdx < 0) return;

        double bestAngle = startAngle;
        bool found = false;
        int coarseN = (int)(MAX_ANGLE_RADIANS / coarseStep) + 2;

        for (int ci = 0; ci < coarseN; ci++) {
            double angle = normalizeAngle(startAngle + ci * coarseStep);
            gPrediction->determineShotResult(true, angle, pwr, spin);

            auto& cueBall = gPrediction->guiData.balls[0];
            if (cueBall.originalOnTable && !cueBall.onTable) continue;

            auto& tb = gPrediction->guiData.balls[targetIdx];
            if (tb.originalOnTable && !tb.onTable) {
                bestAngle = angle; found = true; break;
            }
        }

        if (found) {
            double fineStart = bestAngle - coarseStep;
            int fineN = (int)(coarseStep * 2.0 / fineStep) + 4;
            for (int fi = 0; fi < fineN; fi++) {
                double angle = normalizeAngle(fineStart + fi * fineStep);
                gPrediction->determineShotResult(true, angle, pwr, spin);

                auto& cueBall = gPrediction->guiData.balls[0];
                if (cueBall.originalOnTable && !cueBall.onTable) continue;

                auto& tb = gPrediction->guiData.balls[targetIdx];
                if (tb.originalOnTable && !tb.onTable) { bestAngle = angle; break; }
            }
            setAimAngle(bestAngle);
            ApplyBestSpin(bestAngle);
        }
    }

    void Draw() {
        ImGuiIO& io = GetIO();
        SetNextWindowPos(ImVec2(io.DisplaySize.x - persistent_int["iAIM_WindowX"],
                                persistent_int["iAIM_WindowY"]),
                         ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        SetNextWindowSize(ImVec2(210, 65), ImGuiCond_FirstUseEver);

        if (Begin("AutoAim", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoSavedSettings)) {
            ImVec2 ws = GetWindowSize();
            float aw = ws.x - GetStyle().WindowPadding.x * 2 - GetStyle().ItemSpacing.x * 2;
            float bw = aw / 3;
            float ah = ws.y - GetStyle().WindowPadding.y * 2;
            float bs = (bw < ah) ? bw : ah;

            if (Button("<", ImVec2(bs, bs))) AIM();
            SameLine(); if (Button("9B", ImVec2(bs, bs))) AIM_9Ball();
            SameLine(); if (Button(">", ImVec2(bs, bs))) AIM();
        } End();
    }
};

