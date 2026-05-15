#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>

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

    void AIM(double angleStep = 0.1f) {
        auto startingAngle = NumberUtils::normalizeDoublePrecision(sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        // Use the actual player classification from the game — never guess
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        // Only aim when we have a valid classification (SOLID or STRIPE)
        if (myclass != Ball::Classification::SOLID &&
            myclass != Ball::Classification::STRIPE) return;

        gPrediction->determineShotResult(true, startingAngle);
        std::vector<int> startingPottedBalls;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                startingPottedBalls.push_back(i);
            }
        }

        for (double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(startingAngle + angleStep)); angle != startingAngle; angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(angle + angleStep))) {
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

            // First ball hit must be the correct target class
            if (isAngleGood && gPrediction->guiData.collision.firstHitBall &&
                gPrediction->guiData.collision.firstHitBall->classification != targetClass)
                isAngleGood = false;

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];
            // Never scratch the cue ball
            if (isAngleGood && cueBall.originalOnTable && !cueBall.onTable) isAngleGood = false;
            // Never accidentally pot the 8-ball before it's time
            if (isAngleGood && !only8BPleft && eightBall.originalOnTable && !eightBall.onTable) isAngleGood = false;

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
