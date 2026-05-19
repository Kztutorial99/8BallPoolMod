#pragma once

// ── Auto Aim — Manual Trigger ─────────────────────────────────────────────────
// Press in-game button → calculate aim once → user shoots manually.
// Press again to recalculate. Does NOT auto-trigger every turn.
// ─────────────────────────────────────────────────────────────────────────────

namespace AutoAim {
    bool bActive                  = false;
    std::atomic<bool> bAimed{false};
    int  aimedBallIdx = -1;   // ball index that was aimed at (for overlay)

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    static void DoAim() {
        if (!sharedGameManager) return;

        auto vc = sharedGameManager.mVisualCue();
        auto vg = vc.mVisualGuide();

        double startAngle = NumberUtils::normalizeDoublePrecision(vg.mAimAngle());
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        gPrediction->determineShotResult(true, startAngle);

        std::vector<int> startPotted;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) startPotted.push_back(i);
        }

        bool only8BLeft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == myclass && ball.originalOnTable) {
                only8BLeft = false;
                break;
            }
        }

        Ball::Classification target = only8BLeft
            ? Ball::Classification::EIGHT_BALL
            : myclass;

        double scanAngle = fmod(startAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);

        while (std::abs(scanAngle - startAngle) > ANGLE_STEP * 0.5) {
            gPrediction->determineShotResult(true, scanAngle);

            bool isGood = false;
            std::vector<int> curPotted;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.classification == target && ball.originalOnTable && !ball.onTable) {
                    curPotted.push_back(i);
                    isGood = true;
                }
            }

            if (isGood && gPrediction->guiData.collision.firstHitBall) {
                if (gPrediction->guiData.collision.firstHitBall->classification != target)
                    isGood = false;
            }

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];
            if (isGood && !cueBall.onTable)                   isGood = false;
            if (isGood && !only8BLeft && !eightBall.onTable)  isGood = false;

            if (isGood && !curPotted.empty() && curPotted != startPotted) {
                vg.mAimAngle(scanAngle);
                if (!curPotted.empty()) aimedBallIdx = curPotted[0];
                break;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
        }
    }

    void TriggerAim() {
        if (!sharedGameManager) return;
        g_autoPlayCalculating = true;
        aimedBallIdx = -1;
        DoAim();
        bAimed                = true;
        g_autoPlayCalculating = false;
    }

    void ResetAimed() {
        bAimed                = false;
        aimedBallIdx          = -1;
        g_autoPlayCalculating = false;
    }

    void Update() {
        if (!bActive || !sharedGameManager) return;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;
        int stateId = gsm.getCurrentStateId();
        if (stateId != 4 && bAimed) ResetAimed();
    }
}
