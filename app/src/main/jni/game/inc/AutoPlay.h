#pragma once

namespace AutoPlay {
    bool bAutoPlaying = false;

    enum ScanState { IDLE, CALCULATING, READY_TO_SHOOT } scanState = IDLE;

    float calcTimer  = 0.0f;
    float shootTimer = 0.0f;

    static constexpr float  CALC_DELAY  = 0.30f;
    static constexpr float  SHOOT_DELAY = 0.40f;
    static constexpr double ANGLE_STEP  = MIN_ANGLE_STEP_RADIANS;

    static void DoAim() {
        auto& vc = sharedGameManager.mVisualCue();
        auto& vg = vc.mVisualGuide();

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

        double scanAngle = fmod(startAngle + ANGLE_STEP + MAX_ANGLE_RADIANS, MAX_ANGLE_RADIANS);
        scanAngle = NumberUtils::normalizeDoublePrecision(scanAngle);

        while (std::abs(scanAngle - startAngle) > ANGLE_STEP * 0.5) {
            gPrediction->determineShotResult(true, scanAngle);

            bool isGood = false;
            std::vector<int> curPotted;
            Ball::Classification target = only8BLeft ? Ball::Classification::EIGHT_BALL : myclass;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.classification == target && ball.originalOnTable && !ball.onTable) {
                    curPotted.push_back(i);
                    isGood = true;
                }
            }

            if (isGood && gPrediction->guiData.collision.firstHitBall) {
                if (gPrediction->guiData.collision.firstHitBall->classification != myclass)
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
        scanState  = IDLE;
        calcTimer  = 0.0f;
        shootTimer = 0.0f;
    }

    void Update() {
        if (!bAutoPlaying || !sharedGameManager) return;

        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;

        int   stateId = gsm.getCurrentStateId();
        float dt      = ImGui::GetIO().DeltaTime;

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
                    scanState  = READY_TO_SHOOT;
                    shootTimer = 0.0f;
                }
                break;

            case READY_TO_SHOOT:
                shootTimer += dt;
                if (shootTimer >= SHOOT_DELAY && !buttonClicker.Active) {
                    buttonClicker.Click(ImVec2(Width * 0.5f, Height * 0.5f), 0.15f);
                    ClearState();
                }
                break;
        }
    }
}
