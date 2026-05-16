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

    // Returns the shot power to use for aim calculations.
    // If player hasn't set power yet (0 / very small), fall back to full CUE max.
    static double aimPower() {
        double p = sharedGameManager.mVisualCue().getShotPower();
        if (p < 50.0) p = CUE_PROPERTIES_MAX_POWER;
        return p;
    }

    // ── Spin search candidates ─────────────────────────────────────────────────
    // After finding the best angle, scan spin variants and pick the one that
    // maximises balls of our class being potted (without scratching or potting 8-ball early).
    static void ApplyBestSpin(double bestAngle, Ball::Classification targetClass, int eightBallIdx) {
        static const Vec2d spinCandidates[] = {
            { 0.0,  0.0 },  // no spin
            { 0.0,  0.7 },  // top
            { 0.0, -0.7 },  // bottom
            { 0.65, 0.0 },  // right
            {-0.65, 0.0 },  // left
            { 0.45, 0.45 }, // top-right
            {-0.45, 0.45 }, // top-left
            { 0.45,-0.45 }, // bottom-right
            {-0.45,-0.45 }, // bottom-left
            { 0.0,  1.0 },  // max top
            { 0.0, -1.0 },  // max bottom
            { 0.9,  0.0 },  // max right
            {-0.9,  0.0 },  // max left
        };

        int bestScore = -999;
        Vec2d bestSpin = {0.0, 0.0};
        double pwr = aimPower();

        for (const auto& spin : spinCandidates) {
            gPrediction->determineShotResult(true, bestAngle, pwr, spin);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue; // scratch — skip

            // Prevent accidentally potting the 8-ball before it's time
            if (eightBallIdx >= 0 &&
                targetClass != Ball::Classification::EIGHT_BALL &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

            int score = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (!b.originalOnTable) continue;
                if (!b.onTable) {
                    if (b.classification == targetClass)   score += 3; // our ball potted
                    else if (b.classification != Ball::Classification::EIGHT_BALL)
                                                           score -= 1; // wrong ball potted
                }
            }
            if (score > bestScore) {
                bestScore = score;
                bestSpin  = spin;
            }
        }

        if (bestScore > 0)
            sharedGameManager.mVisualEnglishControl().mEnglish(bestSpin);
    }

    // ── Targeted ball aim ─────────────────────────────────────────────────────
    // Scans angles to find one where the cue first hits targetIdx and pots it.
    void AIM_Targeted(int targetIdx, double angleStep = 0.05) {
        if (targetIdx < 1 || targetIdx >= gPrediction->guiData.ballsCount) return;
        if (!gPrediction->guiData.balls[targetIdx].originalOnTable) return;

        // Find 8-ball index so we never pot it early
        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        double pwr = aimPower();
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
            if (gPrediction->guiData.collision.firstHitBall->index != targetIdx) continue;

            auto& tgt = gPrediction->guiData.balls[targetIdx];
            if (tgt.originalOnTable && !tgt.onTable) {
                setAimAngle(a);
                // Find best spin for this angle
                Ball::Classification tgtClass = tgt.classification;
                ApplyBestSpin(a, tgtClass, eightBallIdx);
                return;
            }
        }
    }

    // ── Auto Aim Random ───────────────────────────────────────────────────────
    // Picks a random ball of the player's class and finds a potting angle.
    void AIM_Random(double angleStep = 0.05) {
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

    void AIM(double angleStep = 0.05f) {
        // Targeted ball aim
        if (persistent_bool[O("bTargetedAim")]) {
            int idx = persistent_int[O("iTargetBall")];
            if (idx >= 1) { AIM_Targeted(idx, angleStep); return; }
        }

        // Random aim
        if (persistent_bool[O("bRandomAim")]) {
            AIM_Random(angleStep);
            return;
        }

        // ── Default: scan all player balls, pick best potting angle ──────────
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i; break;
            }
        }

        double pwr = aimPower();
        auto startingAngle = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        gPrediction->determineShotResult(true, startingAngle, pwr);
        std::vector<int> startingPottedBalls;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable)
                startingPottedBalls.push_back(i);
        }

        bool only8BPleft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == myclass &&
                gPrediction->guiData.balls[i].originalOnTable) {
                only8BPleft = false; break;
            }
        }
        Ball::Classification targetClass = only8BPleft
            ? Ball::Classification::EIGHT_BALL : myclass;

        const int maxIter = (int)((2.0 * M_PI) / fabs(angleStep)) + 2;
        for (int iter = 0; iter < maxIter; iter++) {
            double angle = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(startingAngle + angleStep * (double)(iter + 1)));

            gPrediction->determineShotResult(true, angle, pwr);

            std::vector<int> currentPottedBalls;
            bool isAngleGood = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.classification == targetClass && ball.originalOnTable && !ball.onTable) {
                    currentPottedBalls.push_back(i);
                    isAngleGood = true;
                }
            }

            if (isAngleGood && gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->classification != targetClass)
                isAngleGood = false;

            auto& cueBall = gPrediction->guiData.balls[0];
            if (isAngleGood && cueBall.originalOnTable && !cueBall.onTable) isAngleGood = false;
            if (isAngleGood && !only8BPleft && eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) isAngleGood = false;

            if (!currentPottedBalls.empty() && startingPottedBalls != currentPottedBalls && isAngleGood) {
                setAimAngle(angle);
                ApplyBestSpin(angle, targetClass, eightBallIdx);
                break;
            }
        }
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
