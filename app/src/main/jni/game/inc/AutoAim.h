#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <ctime>

using namespace ImGui;

constexpr double maxAngle = 360.0 / (180.0 / M_PI);

double normalizeAngle(double angle) {
    double newAngle = angle;
    if (newAngle >= maxAngle) newAngle = fmod(newAngle, maxAngle);
    else if (newAngle < 0) newAngle = maxAngle - fmod(-newAngle, maxAngle);
    return newAngle;
}

namespace AutoAim {
    double lastSetAngle = 0.f;
    bool didSetAngle = false;

    bool shouldAutoAIM() { return !didSetAngle || lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(); }

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    // Always use the current max power for aim calculations so the predicted
    // line matches exactly what happens when the player shoots at full power.
    static double aimPower() {
        return CUE_PROPERTIES_MAX_POWER;
    }

    // ── Spin search candidates ─────────────────────────────────────────────────
    // After finding the best angle, scan spin variants and pick the one that
    // maximises balls of our class being potted (without scratching or potting 8-ball early).
    // Always applies the best spin found — even if no ball is potted, apply no-scratch spin.
    static void ApplyBestSpin(double bestAngle, Ball::Classification targetClass, int eightBallIdx) {
        static const Vec2d spinCandidates[] = {
            { 0.0,  0.0 },   // no spin
            { 0.0,  0.7 },   // top
            { 0.0, -0.7 },   // bottom
            { 0.65, 0.0 },   // right
            {-0.65, 0.0 },   // left
            { 0.45, 0.45 },  // top-right
            {-0.45, 0.45 },  // top-left
            { 0.45,-0.45 },  // bottom-right
            {-0.45,-0.45 },  // bottom-left
            { 0.0,  1.0 },   // max top
            { 0.0, -1.0 },   // max bottom
            { 0.9,  0.0 },   // max right
            {-0.9,  0.0 },   // max left
            { 0.6,  0.6 },   // strong top-right
            {-0.6,  0.6 },   // strong top-left
            { 0.6, -0.6 },   // strong bottom-right
            {-0.6, -0.6 },   // strong bottom-left
        };

        int bestScore  = -9999;
        Vec2d bestSpin = {0.0, 0.0};
        double pwr     = aimPower();

        for (const auto& spin : spinCandidates) {
            gPrediction->determineShotResult(true, bestAngle, pwr, spin);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue; // scratch — skip

            // Prevent accidentally potting the 8-ball before it's time
            bool eightPotted = (eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable);
            if (targetClass != Ball::Classification::EIGHT_BALL && eightPotted) continue;

            int score = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (!b.originalOnTable) continue;
                if (!b.onTable) {
                    if (b.classification == targetClass)   score += 3;
                    else if (b.classification != Ball::Classification::EIGHT_BALL)
                                                           score -= 2;
                }
            }
            // Bonus: cue ball safe position
            score += 1; // prefer any non-scratch spin

            if (score > bestScore) {
                bestScore = score;
                bestSpin  = spin;
            }
        }

        // Always apply — even if bestScore <= 0, the best non-scratch spin is used
        sharedGameManager.mVisualEnglishControl().mEnglish(bestSpin);
    }

    // ── Targeted ball aim ─────────────────────────────────────────────────────
    // Scans angles in multiple passes (coarse→fine) to find the best shot angle
    // where the cue first hits targetIdx and pots it.
    // Ball 8 CAN be targeted directly — restriction is removed when explicitly chosen.
    void AIM_Targeted(int targetIdx, double angleStep = 0.02) {
        if (targetIdx < 1 || targetIdx >= gPrediction->guiData.ballsCount) return;
        if (!gPrediction->guiData.balls[targetIdx].originalOnTable) return;

        bool targetIs8Ball = (gPrediction->guiData.balls[targetIdx].classification ==
                              Ball::Classification::EIGHT_BALL);

        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        double pwr = aimPower();

        // Multi-pass: coarse scan first for speed, then refine around best candidate
        double bestAngle  = -1.0;
        double coarseStep = 0.08;
        double fineStep   = 0.015;

        // ── Pass 1: coarse scan ──────────────────────────────────────────────
        double startingAngle = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        const int coarseMax = (int)((2.0 * M_PI) / coarseStep) + 2;
        for (int iter = 0; iter < coarseMax; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(startingAngle + coarseStep * (double)(iter + 1)));

            gPrediction->determineShotResult(true, a, pwr);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (gPrediction->guiData.collision.firstHitBall->index != targetIdx) continue;

            // Allow 8-ball potting only if it's the explicit target
            if (!targetIs8Ball && eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

            auto& tgt = gPrediction->guiData.balls[targetIdx];
            if (tgt.originalOnTable && !tgt.onTable) {
                bestAngle = a;
                break;
            }
        }

        // ── Pass 2: fine scan around best coarse angle (±coarseStep range) ──
        if (bestAngle >= 0.0) {
            double fineStart = bestAngle - coarseStep;
            double fineEnd   = bestAngle + coarseStep;
            double refinedBest = bestAngle;

            const int fineMax = (int)((fineEnd - fineStart) / fineStep) + 2;
            for (int iter = 0; iter < fineMax; iter++) {
                double a = NumberUtils::normalizeDoublePrecision(
                    normalizeAngle(fineStart + fineStep * (double)iter));

                gPrediction->determineShotResult(true, a, pwr);
                auto& cue = gPrediction->guiData.balls[0];
                if (!cue.onTable) continue;
                if (!gPrediction->guiData.collision.firstHitBall) continue;
                if (gPrediction->guiData.collision.firstHitBall->index != targetIdx) continue;

                if (!targetIs8Ball && eightBallIdx >= 0 &&
                    gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                    !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

                auto& tgt = gPrediction->guiData.balls[targetIdx];
                if (tgt.originalOnTable && !tgt.onTable) {
                    refinedBest = a;
                    break;
                }
            }
            bestAngle = refinedBest;
        }

        // ── Pass 3: fallback — full fine scan if coarse failed ───────────────
        if (bestAngle < 0.0) {
            const int fineMax2 = (int)((2.0 * M_PI) / fineStep) + 2;
            for (int iter = 0; iter < fineMax2; iter++) {
                double a = NumberUtils::normalizeDoublePrecision(
                    normalizeAngle(startingAngle + fineStep * (double)(iter + 1)));

                gPrediction->determineShotResult(true, a, pwr);
                auto& cue = gPrediction->guiData.balls[0];
                if (!cue.onTable) continue;
                if (!gPrediction->guiData.collision.firstHitBall) continue;
                if (gPrediction->guiData.collision.firstHitBall->index != targetIdx) continue;

                if (!targetIs8Ball && eightBallIdx >= 0 &&
                    gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                    !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

                auto& tgt = gPrediction->guiData.balls[targetIdx];
                if (tgt.originalOnTable && !tgt.onTable) {
                    bestAngle = a;
                    break;
                }
            }
        }

        if (bestAngle >= 0.0) {
            setAimAngle(bestAngle);
            Ball::Classification tgtClass = gPrediction->guiData.balls[targetIdx].classification;
            ApplyBestSpin(bestAngle, tgtClass, eightBallIdx);
        }
    }

    // ── Auto Aim Best Shot ────────────────────────────────────────────────────
    // Scans ALL balls of player's class and picks the angle that pots the most.
    // Multi-pass: coarse first, then refines the best window found.
    void AIM_BestShot(double coarseStep = 0.08, double fineStep = 0.015) {
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        bool canAimAny = (myclass == Ball::Classification::SOLID   ||
                          myclass == Ball::Classification::STRIPE  ||
                          myclass == Ball::Classification::ANY);
        if (!canAimAny) return;

        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        bool only8BPleft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable) {
                only8BPleft = false; break;
            }
        }
        Ball::Classification targetClass = only8BPleft
            ? Ball::Classification::EIGHT_BALL : myclass;

        double pwr = aimPower();
        double startingAngle = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        // ── Coarse scan: find best scoring angle ──────────────────────────────
        int    bestScore       = -1;
        double bestAngle       = -1.0;
        double bestWindowStart = -1.0;

        const int coarseMax = (int)((2.0 * M_PI) / coarseStep) + 2;
        for (int iter = 0; iter < coarseMax; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(startingAngle + coarseStep * (double)(iter + 1)));

            gPrediction->determineShotResult(true, a, pwr);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (!only8BPleft &&
                gPrediction->guiData.collision.firstHitBall->classification != targetClass) continue;
            if (!only8BPleft && eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

            int score = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.classification == targetClass && b.originalOnTable && !b.onTable)
                    score++;
            }
            if (score > bestScore) {
                bestScore       = score;
                bestAngle       = a;
                bestWindowStart = a - coarseStep;
            }
        }

        if (bestScore < 1) return; // nothing found even coarsely

        // ── Fine scan: refine around best coarse window ───────────────────────
        double fineWindowEnd = bestAngle + coarseStep;
        int    fineBestScore = bestScore;
        double fineBestAngle = bestAngle;

        const int fineMax = (int)((fineWindowEnd - bestWindowStart) / fineStep) + 2;
        for (int iter = 0; iter < fineMax; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(bestWindowStart + fineStep * (double)iter));

            gPrediction->determineShotResult(true, a, pwr);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (!only8BPleft &&
                gPrediction->guiData.collision.firstHitBall->classification != targetClass) continue;
            if (!only8BPleft && eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

            int score = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.classification == targetClass && b.originalOnTable && !b.onTable)
                    score++;
            }
            if (score >= fineBestScore) {
                fineBestScore = score;
                fineBestAngle = a;
            }
        }

        setAimAngle(fineBestAngle);
        ApplyBestSpin(fineBestAngle, targetClass, eightBallIdx);
    }

    // ── Auto Aim Random ───────────────────────────────────────────────────────
    // Tries each ball of player's class, picks a potting angle with best spin.
    void AIM_Random(double angleStep = 0.02) {
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        std::vector<int> candidates;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable)
                candidates.push_back(i);
        }

        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        // If all our balls potted → aim at 8-ball to win
        if (candidates.empty()) {
            if (eightBallIdx >= 0 && gPrediction->guiData.balls[eightBallIdx].originalOnTable) {
                AIM_Targeted(eightBallIdx, angleStep);
            }
            return;
        }

        double pwr = aimPower();
        int startIdx = (int)((int64_t)time(nullptr) % (int64_t)candidates.size());

        for (int attempt = 0; attempt < (int)candidates.size(); attempt++) {
            int ballIdx = candidates[(startIdx + attempt) % (int)candidates.size()];

            double startingAngle = NumberUtils::normalizeDoublePrecision(
                sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

            const int maxIter = (int)((2.0 * M_PI) / fabs(angleStep)) + 2;
            for (int iter = 0; iter < maxIter; iter++) {
                double a = NumberUtils::normalizeDoublePrecision(
                    normalizeAngle(startingAngle + angleStep * (double)(iter + 1)));

                gPrediction->determineShotResult(true, a, pwr);
                auto& cue = gPrediction->guiData.balls[0];
                if (!cue.onTable) continue;

                if (!gPrediction->guiData.collision.firstHitBall) continue;
                if (gPrediction->guiData.collision.firstHitBall->index != ballIdx) continue;

                // Never pot 8-ball early
                if (eightBallIdx >= 0 &&
                    gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                    !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

                auto& tgt = gPrediction->guiData.balls[ballIdx];
                if (tgt.originalOnTable && !tgt.onTable) {
                    setAimAngle(a);
                    ApplyBestSpin(a, myclass, eightBallIdx);
                    return;
                }
            }
        }
    }

    void AIM(double angleStep = 0.02) {
        // Targeted ball aim — supports ball 8 explicitly
        if (persistent_bool[O("bTargetedAim")]) {
            int idx = persistent_int[O("iTargetBall")];
            if (idx >= 1) { AIM_Targeted(idx, angleStep); return; }
        }

        // Random ball aim
        if (persistent_bool[O("bRandomAim")]) {
            AIM_Random(angleStep);
            return;
        }

        // Default: best-shot scanner — picks angle that pots the most
        AIM_BestShot();
    }

    void Draw() {
        ImGuiIO& io = GetIO();
        SetNextWindowPos(ImVec2(io.DisplaySize.x - persistent_int["iAIM_WindowX"], persistent_int["iAIM_WindowY"]), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        SetNextWindowSize(ImVec2(210, 65), ImGuiCond_FirstUseEver);

        if (Begin("AutoAim", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
            ImVec2 windowSize = GetWindowSize();
            float availableWidth = windowSize.x - GetStyle().WindowPadding.x * 2 - GetStyle().ItemSpacing.x * 2;
            float buttonWidth = availableWidth / 3;
            float availableHeight = windowSize.y - GetStyle().WindowPadding.y * 2;
            float buttonSize = (buttonWidth < availableHeight) ? buttonWidth : availableHeight;

            Ball::Classification cls = sharedGameManager.getPlayerClassification();
            const char* clsLabel = (cls == Ball::Classification::SOLID) ? "SOL"
                                 : (cls == Ball::Classification::STRIPE) ? "STR" : "?";
            TextDisabled("%s", clsLabel);
            SameLine(); if (Button("<", ImVec2(buttonSize, buttonSize))) AIM(persistent_float["fAIM_AngleStep"]);
            SameLine(); if (Button(">", ImVec2(buttonSize, buttonSize))) AIM(-persistent_float["fAIM_AngleStep"]);
        } End();
    }
};
