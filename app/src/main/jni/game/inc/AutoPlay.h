#pragma once

namespace AutoAim {
    bool bActive = false;

    enum ScanState { IDLE, CALCULATING, AIMED } scanState = IDLE;
    float calcTimer = 0.0f;

    static constexpr float  CALC_DELAY  = 0.25f;
    static constexpr double ANGLE_STEP  = MIN_ANGLE_STEP_RADIANS;

    static void DoAim() {
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

        // Check if only the 8-ball remains for this player
        bool only8BLeft = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == myclass && ball.originalOnTable) {
                only8BLeft = false;
                break;
            }
        }

        // Target is 8-ball when all other balls are potted, otherwise player's class
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

            // FIX: cue ball must hit the TARGET type first (not myclass — wrong when shooting 8-ball)
            if (isGood && gPrediction->guiData.collision.firstHitBall) {
                if (gPrediction->guiData.collision.firstHitBall->classification != target)
                    isGood = false;
            }

            auto& cueBall   = gPrediction->guiData.balls[0];
            auto& eightBall = gPrediction->guiData.balls[8];
            if (isGood && !cueBall.onTable)                  isGood = false;
            if (isGood && !only8BLeft && !eightBall.onTable) isGood = false;

            if (isGood && !curPotted.empty() && curPotted != startPotted) {
                vg.mAimAngle(scanAngle);
                break;
            }

            scanAngle = fmod(scanAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
            scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);
        }
    }

    void ClearState() {
        scanState = IDLE;
        calcTimer = 0.0f;
    }

    void Update() {
        if (!bActive || !sharedGameManager) return;

        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;

        int   stateId = gsm.getCurrentStateId();
        float dt      = ImGui::GetIO().DeltaTime;

        // Not player's turn — reset so we re-aim at the start of the next turn
        if (stateId != 4) {
            if (scanState != IDLE) ClearState();
            g_autoPlayCalculating = false;
            return;
        }

        switch (scanState) {
            case IDLE:
                calcTimer             = 0.0f;
                scanState             = CALCULATING;
                g_autoPlayCalculating = true;
                break;

            case CALCULATING:
                calcTimer += dt;
                if (calcTimer >= CALC_DELAY) {
                    DoAim();
                    g_autoPlayCalculating = false;
                    scanState = AIMED;
                }
                break;

            case AIMED:
                // Aim locked — user shoots manually.
                // State will reset automatically when turn ends (stateId != 4).
                break;
        }
    }
}
