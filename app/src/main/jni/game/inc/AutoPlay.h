#pragma once

// ── Auto Aim (Manual Trigger) ─────────────────────────────────────────────────
// Behavior: User presses the in-game AIM button → DoAim() runs ONCE →
// aim angle is set in-game → user shoots manually.
// Press again to recalculate. Does NOT auto-trigger every turn.
// ─────────────────────────────────────────────────────────────────────────────

namespace AutoAim {
    bool bActive     = false;  // enabled (shows toggle button in-game)
    bool bAimed      = false;  // true = aim already calculated this trigger
    bool bCalculating = false; // true = currently running DoAim (shows overlay)

    static constexpr double ANGLE_STEP = MIN_ANGLE_STEP_RADIANS;

    // ── Core aim calculation ──────────────────────────────────────────────────
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
                break;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
        }
    }

    // ── Called by in-game button: calculate aim once ──────────────────────────
    void TriggerAim() {
        if (!sharedGameManager) return;
        bCalculating      = true;
        g_autoPlayCalculating = true;
        DoAim();
        bAimed            = true;
        bCalculating      = false;
        g_autoPlayCalculating = false;
    }

    // ── Reset after each shot/turn ────────────────────────────────────────────
    void ResetAimed() {
        bAimed            = false;
        bCalculating      = false;
        g_autoPlayCalculating = false;
    }

    // ── Update: only monitors turn-end to auto-reset bAimed ──────────────────
    // Does NOT auto-calculate. Calculation is manual (TriggerAim).
    void Update() {
        if (!bActive || !sharedGameManager) return;

        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;

        int stateId = gsm.getCurrentStateId();

        // When it's no longer player's turn, clear aimed state
        if (stateId != 4) {
            if (bAimed) ResetAimed();
        }
    }
}
