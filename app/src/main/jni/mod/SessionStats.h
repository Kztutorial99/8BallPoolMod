#pragma once

#include "imgui/inc/persistence.h"
#include "game/GameManager.h"

// ── SessionStats ──────────────────────────────────────────────────────────────
// Tracks in-session statistics by monitoring game state transitions.
// Call SessionStats::Update(stateId) once per frame from DrawESP.
// Call SessionStats::Draw() from DrawMenu to show the HUD overlay.
// ─────────────────────────────────────────────────────────────────────────────
namespace SessionStats {

    static int g_prevStateId   = -1;
    static int g_prevBallCount = -1;

    static int g_shots   = 0;
    static int g_pots    = 0;
    static int g_games   = 0;
    static int g_wins    = 0;
    static int g_fouls   = 0;

    // Count balls currently on table for our classification
    static int CountMyBalls(Ball::Classification myclass) {
        int n = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.classification == myclass && b.originalOnTable) n++;
        }
        return n;
    }

    static void Update(int stateId) {
        if (!sharedGameManager) { g_prevStateId = stateId; return; }

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        bool validClass = (myclass == Ball::Classification::SOLID ||
                           myclass == Ball::Classification::STRIPE ||
                           myclass == Ball::Classification::NINE_BALL_RULE);

        // ── Detect shot: player's turn (4) → balls moving (6/7/8) ──────────
        if (g_prevStateId == 4 && (stateId == 6 || stateId == 7 || stateId == 8)) {
            g_shots++;
            if (validClass) g_prevBallCount = CountMyBalls(myclass);
        }

        // ── Detect pot: balls stopped → our ball count decreased ───────────
        if ((g_prevStateId == 6 || g_prevStateId == 7 || g_prevStateId == 8) &&
            stateId == 4 && validClass && g_prevBallCount >= 0) {
            int nowCount = CountMyBalls(myclass);
            int potted   = g_prevBallCount - nowCount;
            if (potted > 0) g_pots += potted;
            g_prevBallCount = -1;
        }

        // ── Detect game start (state 4 appearing after idle/menu) ──────────
        if (g_prevStateId <= 2 && stateId == 4) {
            g_games++;
        }

        g_prevStateId = stateId;
    }

    static void Reset() {
        g_shots = g_pots = g_games = g_wins = g_fouls = 0;
        g_prevStateId = -1;
        g_prevBallCount = -1;
    }

    // Small HUD overlay — drawn in bottom-left corner
    static void DrawHUD() {
        if (!persistent_bool["bSessionStats"]) return;

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(12.0f, io.DisplaySize.y - 12.0f),
                                ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg,  IM_COL32(10, 12, 20, 210));
        ImGui::PushStyleColor(ImGuiCol_Border,    IM_COL32(60, 60, 90, 180));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        if (ImGui::Begin("##Stats", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(0.75f);

            auto Row = [](const char* lbl, int val, ImU32 col) {
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", lbl);
                ImGui::SameLine();
                char buf[16]; snprintf(buf, sizeof(buf), "%d", val);
                ImGui::TextColored(ImColor(col), "%s", buf);
            };

            Row("Shots ", g_shots, IM_COL32(200, 200, 255, 255));
            Row("Pots  ", g_pots,  IM_COL32(80,  255, 120, 255));
            Row("Games ", g_games, IM_COL32(255, 200, 80,  255));
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

} // namespace SessionStats
