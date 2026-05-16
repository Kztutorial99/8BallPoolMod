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

    // Targeted ball aim — scan for angle that FIRST HITS the target ball AND pots it
    void AIM_Targeted(int targetIdx, double angleStep = 0.1) {
        if (targetIdx < 1 || targetIdx >= gPrediction->guiData.ballsCount) return;
        // Target ball must be on table
        if (!gPrediction->guiData.balls[targetIdx].originalOnTable) return;

        double startingAngle = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        const int maxIter = (int)((2.0 * M_PI) / fabs(angleStep)) + 2;
        for (int iter = 0; iter < maxIter; iter++) {
            double a = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(startingAngle + angleStep * (double)(iter + 1)));

            gPrediction->determineShotResult(true, a);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue; // no scratch

            // The FIRST ball hit MUST be the exact target ball — ensures legal clean shot
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (gPrediction->guiData.collision.firstHitBall->index != targetIdx) continue;

            auto& tgt = gPrediction->guiData.balls[targetIdx];
            if (tgt.originalOnTable && !tgt.onTable) {
                setAimAngle(a);
                return;
            }
        }
    }

    // Auto Aim Random — randomly targets a valid ball of player's class and aims to pot it
    // Only accepts shots where the FIRST ball hit IS the target (solid/clean shot)
    void AIM_Random(double angleStep = 0.1) {
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        // Collect all on-table balls of our class
        std::vector<int> candidates;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable)
                candidates.push_back(i);
        }

        // If all our balls are potted, aim at 8-ball to win
        if (candidates.empty()) {
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.classification == Ball::Classification::EIGHT_BALL && b.originalOnTable) {
                    AIM_Targeted(i, angleStep);
                    return;
                }
            }
            return;
        }

        // Find 8-ball index dynamically
        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i;
                break;
            }
        }

        // Random starting point — changes each second so it picks different balls
        int startIdx = (int)((int64_t)time(nullptr) % (int64_t)candidates.size());

        for (int attempt = 0; attempt < (int)candidates.size(); attempt++) {
            int ballIdx = candidates[(startIdx + attempt) % (int)candidates.size()];

            double startingAngle = NumberUtils::normalizeDoublePrecision(
                sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

            const int maxIter = (int)((2.0 * M_PI) / fabs(angleStep)) + 2;
            for (int iter = 0; iter < maxIter; iter++) {
                double a = NumberUtils::normalizeDoublePrecision(
                    normalizeAngle(startingAngle + angleStep * (double)(iter + 1)));

                gPrediction->determineShotResult(true, a);
                auto& cue = gPrediction->guiData.balls[0];
                if (!cue.onTable) continue; // no scratch

                // First ball hit MUST be our chosen target ball
                if (!gPrediction->guiData.collision.firstHitBall) continue;
                if (gPrediction->guiData.collision.firstHitBall->index != ballIdx) continue;

                // Don't accidentally pot the 8-ball before it's time
                if (eightBallIdx >= 0 &&
                    gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                    !gPrediction->guiData.balls[eightBallIdx].onTable) continue;

                auto& tgt = gPrediction->guiData.balls[ballIdx];
                if (tgt.originalOnTable && !tgt.onTable) {
                    setAimAngle(a);
                    return;
                }
            }
        }
    }

    void AIM(double angleStep = 0.1f) {
        // Targeted ball aim — delegate to AIM_Targeted when enabled
        if (persistent_bool[O("bTargetedAim")]) {
            int idx = persistent_int[O("iTargetBall")];
            if (idx >= 1) { AIM_Targeted(idx, angleStep); return; }
        }

        // Random aim — targets a random solid ball of player's class
        if (persistent_bool[O("bRandomAim")]) {
            AIM_Random(angleStep);
            return;
        }

        auto startingAngle = NumberUtils::normalizeDoublePrecision(sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        // Use the actual player classification from the game
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        // Only aim when we have a valid classification (SOLID or STRIPE)
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        // Find 8-ball index dynamically — works for 8-ball and 16-ball modes
        int eightBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                eightBallIdx = i;
                break;
            }
        }

        gPrediction->determineShotResult(true, startingAngle);
        std::vector<int> startingPottedBalls;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                startingPottedBalls.push_back(i);
            }
        }

        const int maxIter = (int)((2.0 * M_PI) / fabs(angleStep)) + 2;
        for (int iter = 0; iter < maxIter; iter++) {
            double angle = NumberUtils::normalizeDoublePrecision(
                normalizeAngle(startingAngle + angleStep * (double)(iter + 1)));

            gPrediction->determineShotResult(true, angle);

            // Check if all our balls are already potted — only 8-ball left
            bool only8BPleft = true;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                Prediction::Ball& ball = gPrediction->guiData.balls[i];
                if (ball.classification == myclass && ball.originalOnTable) {
                    only8BPleft = false;
                    break;
                }
            }

            // Target classification: our balls normally, 8-ball when all ours are gone
            Ball::Classification targetClass = only8BPleft
                ? Ball::Classification::EIGHT_BALL
                : myclass;

            std::vector<int> currentPottedBalls;
            bool isAngleGood = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                Prediction::Ball& ball = gPrediction->guiData.balls[i];
                if (ball.classification == targetClass) {
                    if (ball.originalOnTable && !ball.onTable) {
                        currentPottedBalls.push_back(i);
                        isAngleGood = true;
                    }
                }
            }

            // First ball hit must be the correct target class — ensures legal shot
            if (isAngleGood && gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->classification != targetClass)
                isAngleGood = false;

            auto& cueBall = gPrediction->guiData.balls[0];
            // Never scratch the cue ball
            if (isAngleGood && cueBall.originalOnTable && !cueBall.onTable) isAngleGood = false;
            // Never accidentally pot the 8-ball before it's time
            if (isAngleGood && !only8BPleft && eightBallIdx >= 0 &&
                gPrediction->guiData.balls[eightBallIdx].originalOnTable &&
                !gPrediction->guiData.balls[eightBallIdx].onTable) isAngleGood = false;

            if (!currentPottedBalls.empty() && startingPottedBalls != currentPottedBalls && isAngleGood) {
                setAimAngle(angle);
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

            // Show actual classification from game — read-only label
            Ball::Classification cls = sharedGameManager.getPlayerClassification();
            const char* clsLabel = (cls == Ball::Classification::SOLID) ? "SOL"
                                 : (cls == Ball::Classification::STRIPE) ? "STR" : "?";
            TextDisabled("%s", clsLabel);
            SameLine(); if (Button("<", ImVec2(buttonSize, buttonSize))) AIM(persistent_float["fAIM_AngleStep"]);
            SameLine(); if (Button(">", ImVec2(buttonSize, buttonSize))) AIM(-persistent_float["fAIM_AngleStep"]);
        } End();
    }
};
