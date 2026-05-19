#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>

#include "icons/icons.h"

using namespace ImGui;
using namespace std;

#include "include/includes.h"

#include "game.h"
#include "game/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "mod/keylogin.h"
#include "mod/PocketSelector.h"
#include "oxorany/oxorany.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <sys/system_properties.h>
#include <ctime>

struct MenuState {
    bool isOpen = false;
    int currentTab = 0;
    float sidebarWidth = 750.0f;
    float animProgress = 0.0f;
    float menuAlpha = 0.0f;
    float menuScale = 0.9f;
    ImVec4 accentColor = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
};
static MenuState g_menu;

// Obfuscated expiry date: 01.01.2027 00:00:00 UTC
static const int64_t EXPIRY_TS = O(1798761600LL);

static bool DEBUG_BYPASS_LOGIN = true;

static float EaseOutBack(float x) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col1, ImU32 col2, bool horizontal = true) {
    if (horizontal) {
        dl->AddRectFilledMultiColor(p1, p2, col1, col2, col2, col1);
    } else {
        dl->AddRectFilledMultiColor(p1, p2, col1, col1, col2, col2);
    }
}

// iconType: 0=pencil(Draw) 1=8ball 2=9ball 3=info
static void DrawTabIcon(ImDrawList* dl, ImVec2 center, float sz, int iconType, bool selected) {
    ImU32 col  = selected ? IM_COL32(255, 255, 255, 245) : IM_COL32(160, 175, 210, 200);
    ImU32 col2 = selected ? IM_COL32(160, 210, 255, 200) : IM_COL32(100, 115, 150, 150);
    float r = sz * 0.42f;

    if (iconType == 0) {
        // Pencil icon: diagonal body + nib
        float hw = sz * 0.09f;
        float plen = sz * 0.38f;
        ImVec2 pA(center.x - plen * 0.55f, center.y + plen * 0.55f);
        ImVec2 pB(center.x + plen * 0.45f, center.y - plen * 0.45f);
        dl->AddLine(pA, pB, col, sz * 0.16f);
        // nib tip triangle
        ImVec2 ptip(pA.x - hw * 0.8f, pA.y + hw * 0.8f);
        dl->AddTriangleFilled(
            ImVec2(pA.x - hw, pA.y),
            ImVec2(pA.x, pA.y - hw),
            ptip, col2);
        // eraser cap
        float cx2 = pB.x + hw * 0.7f, cy2 = pB.y - hw * 0.7f;
        dl->AddRectFilled(
            ImVec2(cx2 - hw, cy2 - hw * 0.6f),
            ImVec2(cx2 + hw, cy2 + hw * 0.6f),
            col2, 1.5f);
    } else if (iconType == 1) {
        // 8 Ball: filled circle + "8"
        dl->AddCircleFilled(center, r, selected ? IM_COL32(30, 30, 40, 230) : IM_COL32(20, 22, 35, 200), 32);
        dl->AddCircle(center, r, col, 32, selected ? 2.0f : 1.5f);
        // small white dot
        dl->AddCircleFilled(ImVec2(center.x + r * 0.28f, center.y - r * 0.28f), r * 0.22f, IM_COL32(255,255,255,180));
        // "8" text centered
        const char* t = "8";
        ImVec2 ts = GImGui->Font->CalcTextSizeA(GImGui->FontSize * 0.82f, FLT_MAX, 0, t);
        dl->AddText(GImGui->Font, GImGui->FontSize * 0.82f,
            ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, t);
    } else if (iconType == 2) {
        // 9 Ball: circle outline + "9"
        dl->AddCircle(center, r, col, 32, selected ? 2.2f : 1.8f);
        dl->AddCircleFilled(center, r * 0.6f, selected ? IM_COL32(40, 60, 100, 180) : IM_COL32(25, 35, 60, 150), 24);
        const char* t = "9";
        ImVec2 ts = GImGui->Font->CalcTextSizeA(GImGui->FontSize * 0.82f, FLT_MAX, 0, t);
        dl->AddText(GImGui->Font, GImGui->FontSize * 0.82f,
            ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, t);
    } else {
        // Info: circle + "i"
        dl->AddCircle(center, r, col, 32, selected ? 2.2f : 1.8f);
        // dot above
        dl->AddCircleFilled(ImVec2(center.x, center.y - r * 0.38f), sz * 0.07f, col);
        // bar below
        dl->AddRectFilled(
            ImVec2(center.x - sz * 0.07f, center.y - r * 0.12f),
            ImVec2(center.x + sz * 0.07f, center.y + r * 0.50f),
            col, 2.0f);
    }
}

static bool SidebarButton(const char* label, int iconType, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float iconSize = 42.0f;
    float vPad     = 11.0f;
    float btnH     = vPad + iconSize + 5.0f + g.FontSize * 0.88f + vPad;

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, btnH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // Hover feedback
    if (hovered && !selected) {
        dl->AddRectFilled(
            ImVec2(bb.Min.x + 5.0f, bb.Min.y + 3.0f),
            ImVec2(bb.Max.x - 5.0f, bb.Max.y - 3.0f),
            IM_COL32(255, 255, 255, 16), 14.0f);
    }

    float iconBgR = iconSize * 0.5f + 8.0f;
    ImVec2 iconCenter = ImVec2(
        bb.Min.x + width * 0.5f,
        bb.Min.y + vPad + iconSize * 0.5f
    );

    // Selected: rounded pill with gradient
    if (selected) {
        ImVec2 pMin = ImVec2(iconCenter.x - iconBgR, iconCenter.y - iconBgR);
        ImVec2 pMax = ImVec2(iconCenter.x + iconBgR, iconCenter.y + iconBgR);
        dl->AddRectFilledMultiColor(pMin, pMax,
            IM_COL32(22, 95, 218, 235), IM_COL32(32, 125, 248, 235),
            IM_COL32(24, 108, 230, 235), IM_COL32(18, 85, 205, 235));
        dl->AddRect(pMin, pMax, IM_COL32(90, 170, 255, 80), 14.0f, 0, 1.0f);
    }

    // Vector icon
    DrawTabIcon(dl, iconCenter, iconSize, iconType, selected);

    // Label
    ImVec2 labelSize = CalcTextSize(label);
    ImVec2 textPos = ImVec2(
        bb.Min.x + (width - labelSize.x) * 0.5f,
        bb.Min.y + vPad + iconSize + 5.0f
    );
    ImU32 textCol = selected
        ? IM_COL32(225, 238, 255, 255)
        : IM_COL32(110, 125, 155, 210);
    dl->AddText(textPos, textCol, label);

    // Active dot
    if (selected) {
        dl->AddCircleFilled(
            ImVec2(bb.Min.x + width * 0.5f, bb.Max.y - 4.0f),
            2.5f, IM_COL32(110, 185, 255, 210));
    }

    return pressed;
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float scale  = 1.18f;
    float height = 28.0f * scale;
    float width  = 52.0f * scale;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    float rowH = ImMax(height, textSize.y + 2.0f) + style.FramePadding.y * 2.0f + 8.0f;
    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, rowH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> switchAnim;
    float& animT = switchAnim[id];
    float targetT = *v ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 16.0f;

    ImDrawList* dl = window->DrawList;

    // Row hover highlight
    if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, 10), 8.0f);
    }

    // Left accent bar when ON
    if (animT > 0.05f) {
        float barH = size.y * 0.55f;
        float barY = bb.Min.y + (size.y - barH) * 0.5f;
        dl->AddRectFilled(
            ImVec2(bb.Min.x, barY),
            ImVec2(bb.Min.x + 2.5f, barY + barH),
            IM_COL32(40, 140, 255, (int)(animT * 200)), 2.0f);
    }

    // Toggle track
    ImVec2 togglePos = ImVec2(bb.Max.x - width - 14.0f, bb.Min.y + (rowH - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + width, togglePos.y + height);

    ImVec4 offColor = ImVec4(0.22f, 0.24f, 0.30f, 1.0f);
    ImVec4 onColor  = ImVec4(0.10f, 0.42f, 0.92f, 1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, animT);
    // Track shadow
    dl->AddRectFilled(
        ImVec2(togglePos.x + 1, togglePos.y + 2),
        ImVec2(toggleEnd.x + 1, toggleEnd.y + 2),
        IM_COL32(0, 0, 0, 60), radius);
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);

    // Knob
    float knobX = togglePos.x + radius + (width - height) * animT;
    float knobY  = togglePos.y + radius;
    float knobR  = radius - 3.5f;
    dl->AddCircleFilled(ImVec2(knobX, knobY + 1.5f), knobR, IM_COL32(0, 0, 0, 50));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));
    // Knob shine
    dl->AddCircleFilled(ImVec2(knobX - knobR * 0.25f, knobY - knobR * 0.28f),
        knobR * 0.38f, IM_COL32(255, 255, 255, 80));

    // Label
    float labelY = bb.Min.y + (rowH - textSize.y) * 0.5f;
    ImU32 labelCol = *v ? IM_COL32(230, 238, 255, 255) : IM_COL32(165, 172, 195, 230);
    dl->AddText(ImVec2(bb.Min.x + 16.0f, labelY), labelCol, label);

    return pressed;
}

// Reads an IL2CPP/Unity NSString (UTF-16 internal buffer at offset 0x14, length at 0x10)
static std::string ReadNSString(ptr str) {
    if (!str) return "null";
    int32_t len = F(int32_t, str + 0x10);
    if (len <= 0 || len > 512) return "?";
    std::string result;
    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        uint16_t ch = F(uint16_t, str + 0x14 + i * 2);
        result += (ch > 0 && ch < 128) ? (char)ch : '?';
    }
    return result;
}

// Set true by AutoPlay when scanning — shows CALCULATING overlay
static std::atomic<bool> g_autoPlayCalculating{false};

// Game state flags — set by DrawESP (inside sigsetjmp), read by DrawMenu (outside)
static bool g_espStateReady = false;
static bool g_espIsInGame   = false;

// Pocket selector for Aim Predict (8 Ball): -1 = auto, 0-5 = specific pocket
static int g_selectedPocket8 = -1;

#include "mod/ButtonClicker.h"
#include "game/inc/AutoPlay.h"
#include "game/inc/BreakSpecial.h"
#include "game/inc/AimSafety.h"
#include "game/inc/AimLockTarget.h"
#include "game/inc/AimBreak.h"
#include "game/inc/AimLock8Ball.h"
#include "game/inc/Aim9Ball.h"
#include "game/inc/Aim9BallBreak.h"
#include <thread>
#include <atomic>

// Active aim mode
enum class AimMode : int {
    EIGHTBALL_SAFETY      = 7,
    NONE                  = 0,
    EIGHTBALL_PREDICT     = 1,
    EIGHTBALL_BREAK       = 2,
    NINEBALL_PREDICT      = 3,
    NINEBALL_BREAK        = 4,
    EIGHTBALL_8LOCK       = 5,
    EIGHTBALL_POCKET_LOCK = 6,
};
static AimMode g_aimMode = AimMode::EIGHTBALL_PREDICT;

// Background aim thread guard — prevents game freeze while scanning angles
static std::atomic<bool> g_aimThreadRunning{false};


static bool IsExpired() {
    return (int64_t)time(nullptr) >= EXPIRY_TS;
}

INLINE void DrawExpired(ImGuiIO& io) {
    float winW = g_menu.sidebarWidth;

    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    SetNextWindowSize(ImVec2(winW, 0), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 30.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ExpiredWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize)) {

        SetWindowFontScale(1.6f);
        ImVec2 titleSz = CalcTextSize(O("MOD EXPIRED"));
        SetCursorPosX((winW - 60.0f - titleSz.x) * 0.5f);
        TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "%s", O("MOD EXPIRED"));
        SetWindowFontScale(1.0f);

        Dummy(ImVec2(0, 16));

        PushTextWrapPos(GetCursorPosX() + winW - 60.0f);
        TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s",
            O("Beta Version Expired. Update on our Telegram t.me/Lyn4xp"));
        PopTextWrapPos();

        Dummy(ImVec2(0, 10));
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}

static void DrawToggleButton(); // forward declaration — defined after DrawFloatingButton

INLINE void DrawESP(ImDrawList* draw) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (!sharedGameManager) return;

        UpdateScreenTable();

        sharedDirector = F(ptr, libmain + O(0x4f06288));
        if (!sharedDirector) return;

        sharedUserInfo = F(ptr, libmain + O(0x4e9feb8));
        if (!sharedUserInfo) return;

        F(bool, sharedUserInfo + 0x340) = true;

        sharedMainManager = F(ptr, libmain + O(0x4dde3e0));
        if (!sharedMainManager) return;

        sharedMenuManager = F(ptr, libmain + O(0x4dfe838));
        if (!sharedMenuManager) return;

        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;

        // Export game state for DrawMenu (used outside sigsetjmp guard)
        g_espIsInGame   = mainStateManager.isInGame();
        g_espStateReady = true;

        if (!g_espIsInGame) return;

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto pockets = tableProperties.mPockets();

        // Feed posisi pocket aktual game ke PocketSelector agar hit-test & circle akurat
        {
            Vec2d tmp[TABLE_POCKETS_COUNT];
            for (int i = 0; i < TABLE_POCKETS_COUNT; i++) tmp[i] = pockets[i];
            PocketSelector::SetLivePockets(tmp, TABLE_POCKETS_COUNT);
        }

        if (persistent_bool[O("bESP_DrawPockets")]) {
            for (int i = 0; i < 6; i++) {
                auto screenPos = WorldToScreen(pockets[i]);
                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 40, WHITE, 0, 3.f);
            }
        }

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        if (!gPrediction) return;

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4 && !g_aimThreadRunning.load()) gPrediction->determineShotResult(false);

        // Enemy Line: gambar prediksi tembakan lawan saat giliran lawan (state 7)
        if (stateId == 7 && persistent_bool[O("bESP_EnemyLine")] && !g_aimThreadRunning.load()) {
            gPrediction->determineShotResult(false);
            float lineThick = (float)persistent_int[O("iLineThickness")];
            if (lineThick < 1.f) lineThick = 1.f;
            ImU32 enemyCol = IM_COL32(255, 55, 55, 210);
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 lastPos{};
                    for (size_t j = 1; j < ball.positions.size(); j++) {
                        auto point = WorldToScreen(ball.positions[j]);
                        if (lastPos.x || lastPos.y) draw->AddLine(lastPos, point, enemyCol, lineThick);
                        lastPos = point;
                    }
                    float circleR = lineThick + 1.f;
                    if (circleR < 2.f) circleR = 2.f;
                    draw->AddCircleFilled(WorldToScreen(ball.initialPosition), circleR, enemyCol);
                    draw->AddCircleFilled(WorldToScreen(ball.predictedPosition), 14, IM_COL32(255, 100, 100, 180));
                }
            }
            return;
        }

        if (stateId == 6 || stateId == 7 || stateId == 8) return;

        if (persistent_bool[O("bESP_DrawPocketsShotState")]) {
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 30, GREEN, 0, 5.f);
                }
            }
        }

        // ── Show Pockets: lingkaran bernomor di setiap pocket + lock indicator ──
        if (persistent_bool[O("bShowPockets")]) {
            int lockedPkt = PocketSelector::Get();
            for (int i = 0; i < 6; i++) {
                ImVec2 ps = WorldToScreen(pockets[i]);
                bool isLocked = (i == lockedPkt);

                float r           = isLocked ? 32.0f : 26.0f;
                float borderThick = isLocked ? 3.0f  : 1.8f;

                ImU32 fillCol   = isLocked ? IM_COL32(80, 55, 0, 130) : IM_COL32(10, 20, 60, 90);
                ImU32 borderCol = isLocked ? IM_COL32(255, 210, 30, 230) : IM_COL32(160, 190, 255, 180);
                ImU32 numCol    = isLocked ? IM_COL32(255, 218, 40, 255) : IM_COL32(210, 225, 255, 210);

                draw->AddCircleFilled(ps, r, fillCol);
                draw->AddCircle(ps, r, borderCol, 0, borderThick);

                // Nomor pocket di tengah lingkaran
                char numBuf[4];
                snprintf(numBuf, sizeof(numBuf), "%d", i);
                ImVec2 ts = GImGui->Font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, numBuf);
                draw->AddText(ImVec2(ps.x - ts.x * 0.5f, ps.y - ts.y * 0.5f), numCol, numBuf);

                // Label "LOCK" di bawah pocket terkunci
                if (isLocked) {
                    const char* lbl = "LOCK";
                    ImVec2 lts = GImGui->Font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, lbl);
                    draw->AddText(ImVec2(ps.x - lts.x * 0.5f, ps.y + r + 4.0f),
                                  IM_COL32(255, 210, 30, 230), lbl);
                }
            }
        }

        if (persistent_bool[O("bESP_DrawPredictionLine")] && !g_aimThreadRunning.load()) {
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 lastPos{};
                    float lineThick = (float)persistent_int[O("iLineThickness")];
                    if (lineThick < 1.f) lineThick = 1.f;
                    for (int j = 1; j < ball.positions.size(); j++) {
                        auto point = WorldToScreen(ball.positions[j]);
                        if (lastPos.x || lastPos.y) draw->AddLine(lastPos, point, colors[i], lineThick);
                        lastPos = point;
                    }
                }
            }
        }

        if (persistent_bool[O("bESP_DrawPredictionLine")] && !g_aimThreadRunning.load()) {
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    float circleR = (float)persistent_int[O("iLineThickness")] + 1.f;
                    if (circleR < 2.f) circleR = 2.f;
                    draw->AddCircleFilled(WorldToScreen(ball.initialPosition), circleR, colors[i]);
                    draw->AddCircleFilled(WorldToScreen(ball.predictedPosition), 16, colors[i]);
                }
            }
        }
    }
}

static void DrawSidebar(float sidebarW) {
    ImGuiContext& g  = *GImGui;
    ImDrawList*   dl = GetWindowDrawList();
    ImVec2        wp = GetWindowPos();

    float closeSize = 35.0f;
    float closeBtnW = 70.0f;
    float tabsW     = sidebarW - closeBtnW;
    float btnW      = tabsW / 4.0f;
    float marginB   = 12.0f;

    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    BeginGroup();
    SetCursorPos(ImVec2(0.0f, 0.0f));
    if (SidebarButton(O("Draw"),   0, g_menu.currentTab == 0, btnW)) g_menu.currentTab = 0;
    SameLine(0, 0);
    if (SidebarButton(O("8 Ball"), 1, g_menu.currentTab == 1, btnW)) g_menu.currentTab = 1;
    SameLine(0, 0);
    if (SidebarButton(O("9 Ball"), 2, g_menu.currentTab == 2, btnW)) g_menu.currentTab = 2;
    SameLine(0, 0);
    if (SidebarButton(O("Info"),   3, g_menu.currentTab == 3, btnW)) g_menu.currentTab = 3;
    EndGroup();

    // Measure actual rendered height — this is the true wrap_content
    float sidebarH = GetItemRectMax().y - wp.y;

    // Now draw background on channel 0 (behind the buttons)
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + sidebarW, wp.y + sidebarH),
        IM_COL32(8, 22, 52, 255), IM_COL32(12, 32, 75, 255),
        IM_COL32(12, 32, 75, 255), IM_COL32(8, 22, 52, 255));
    dl->ChannelsMerge();

    // Vertical separator between Queue and close strip
    float sepX       = wp.x + sidebarW - closeBtnW;
    float sepCenterY = wp.y + sidebarH * 0.5f;
    float sepHalfH   = sidebarH * 0.28f;
    dl->AddLine(
        ImVec2(sepX, sepCenterY - sepHalfH),
        ImVec2(sepX, sepCenterY + sepHalfH),
        IM_COL32(40, 100, 220, 180), 1.5f
    );

    // Close (X) button — truly centered in the measured sidebarH
    float closePosX = (sidebarW - closeBtnW) + (closeBtnW - closeSize) * 0.5f;
    float closePosY = (sidebarH - closeSize) * 0.5f;
    SetCursorPos(ImVec2(closePosX, closePosY));
    {
        ImGuiWindow* win = GetCurrentWindow();
        ImGuiID closeId  = win->GetID(O("##CloseMenu"));
        ImVec2 closePos  = win->DC.CursorPos;
        ImRect closeBb(closePos, closePos + ImVec2(closeSize, closeSize));
        ItemSize(ImVec2(closeSize, closeSize), g.Style.FramePadding.y);
        ItemAdd(closeBb, closeId);
        bool closeHovered = false, closeHeld = false;
        bool closePressed = ButtonBehavior(closeBb, closeId, &closeHovered, &closeHeld);
        if (closePressed) g_menu.isOpen = false;

        float xCX = closeBb.Min.x + closeSize * 0.5f;
        float xCY = closeBb.Min.y + closeSize * 0.5f;
        float xH  = closeSize * 0.32f;
        ImU32 xCol = closeHovered ? IM_COL32(255, 255, 255, 240) : IM_COL32(160, 160, 170, 200);
        dl->AddLine(ImVec2(xCX - xH, xCY - xH), ImVec2(xCX + xH, xCY + xH), xCol, 2.2f);
        dl->AddLine(ImVec2(xCX + xH, xCY - xH), ImVec2(xCX - xH, xCY + xH), xCol, 2.2f);
    }

    // Bottom margin — cursor pushed past the true sidebar height
    SetCursorPos(ImVec2(0.0f, sidebarH));
    Dummy(ImVec2(sidebarW, marginB));
}

// Shared vertical position for DrawToggleButton and DrawFloatingButton (they move together)
static float g_sideBtnsY      = 0.0f;
// Kept for linker compatibility — no longer used for animation
static float g_toggleRotAngle = 0.0f;

// ── svConfig ──────────────────────────────────────────────────────────────────
static void svConfig_Save() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("w"));
    if (!f) return;
    fprintf(f, O("iLineThickness=%d\n"),  persistent_int[O("iLineThickness")]);
    fprintf(f, O("iMenuSizeOffset=%d\n"), persistent_int[O("iMenuSizeOffset")]);
    fclose(f);
}
static void svConfig_Load() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("r"));
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        if (sscanf(line, O("iLineThickness=%d"),  &v) == 1) { persistent_int[O("iLineThickness")]  = v; continue; }
        if (sscanf(line, O("iMenuSizeOffset=%d"), &v) == 1) { persistent_int[O("iMenuSizeOffset")] = v; }
    }
    fclose(f);
}

// ── Aim Info Overlay — 2 baris simpel, text-only, auto-hide ──────────────────
static void DrawAimInfoOverlay(ImGuiIO& io) {
    bool isAnalyzing = g_autoPlayCalculating.load() || g_aimThreadRunning.load();
    bool isAimed     = AutoAim::bAimed.load();
    if (!isAnalyzing && !isAimed) return;

    float t = (float)GetTime();

    // ── Mode name & color (plain literals — NEVER O() untuk pointer yang disimpan) ──
    ImVec4 modeColor = ImVec4(0.50f, 0.70f, 1.0f, 1.0f);
    char modeName[8] = "AIM";
    switch (g_aimMode) {
        case AimMode::EIGHTBALL_PREDICT: strcpy(modeName,"8BP"); modeColor=ImVec4(0.25f,0.60f,1.0f,1.0f); break;
        case AimMode::EIGHTBALL_BREAK:   strcpy(modeName,"8BK"); modeColor=ImVec4(1.0f, 0.50f,0.10f,1.0f); break;
        case AimMode::EIGHTBALL_8LOCK:       strcpy(modeName,"LK8"); modeColor=ImVec4(0.90f,0.20f,0.90f,1.0f); break;
        case AimMode::EIGHTBALL_POCKET_LOCK: strcpy(modeName,"PKT"); modeColor=ImVec4(1.0f, 0.75f,0.10f,1.0f); break;
        case AimMode::NINEBALL_PREDICT:      strcpy(modeName,"9BP"); modeColor=ImVec4(0.15f,0.85f,0.55f,1.0f); break;
        case AimMode::NINEBALL_BREAK:    strcpy(modeName,"9GW"); modeColor=ImVec4(1.0f, 0.82f,0.05f,1.0f); break;
        case AimMode::EIGHTBALL_SAFETY:  strcpy(modeName,"SFT"); modeColor=ImVec4(0.10f,0.88f,0.72f,1.0f); break;
        default: break;
    }

    // ── Baris 1: [MODE] STATUS ───────────────────────────────────────────
    char row1[64];
    if (isAnalyzing)
        snprintf(row1, sizeof(row1), "[%s] ANALYZING...", modeName);
    else
        snprintf(row1, sizeof(row1), "[%s] AIMED", modeName);

    ImVec4 statusColor = isAnalyzing
        ? ImVec4(1.0f, 0.78f, 0.0f, 1.0f)
        : ImVec4(0.15f, 1.0f, 0.42f, 1.0f);
    float statusAlpha = isAnalyzing ? 0.90f : 0.88f;

    // ── Baris 2: info singkat (plain snprintf, no O() format) ─────────────
    char row2[64] = "";
    switch (g_aimMode) {
        case AimMode::EIGHTBALL_PREDICT:
            if (isAimed && !AimLockTarget::lastHadShot)
                snprintf(row2, sizeof(row2), "No shot");
            else if (AimLockTarget::lastTargetBall >= 0)
                snprintf(row2, sizeof(row2), "Ball %d  Pkt %d",
                    AimLockTarget::lastTargetBall, AimLockTarget::lastTargetPocket);
            break;
        case AimMode::EIGHTBALL_BREAK:
            if (AimBreak::lastBestCount > 0)
                snprintf(row2, sizeof(row2), "%d balls", AimBreak::lastBestCount);
            break;
        case AimMode::EIGHTBALL_8LOCK:
            if (isAimed && !AimLock8Ball::lastHadShot)
                snprintf(row2, sizeof(row2), "No shot");
            else if (AimLock8Ball::lastTargetPocket >= 0)
                snprintf(row2, sizeof(row2), "Ball 8  Pkt %d", AimLock8Ball::lastTargetPocket);
            break;
        case AimMode::EIGHTBALL_POCKET_LOCK: {
            int fp = PocketSelector::Get();
            if (fp >= 0) {
                const char* pN[6] = {"TL","TC","TR","BR","BC","BL"};
                if (isAimed && !AimLockTarget::lastHadShot)
                    snprintf(row2, sizeof(row2), "No shot  Pkt:%s", pN[fp]);
                else if (AimLockTarget::lastTargetBall >= 0)
                    snprintf(row2, sizeof(row2), "B%d -> Pkt %s",
                        AimLockTarget::lastTargetBall, pN[fp]);
            } else {
                snprintf(row2, sizeof(row2), "No pocket locked");
            }
            break;
        }
        case AimMode::NINEBALL_PREDICT:
            if (Aim9Ball::lastTargetBall >= 0)
                snprintf(row2, sizeof(row2), "Ball %d", Aim9Ball::lastTargetBall);
            break;
        case AimMode::NINEBALL_BREAK:
            if (Aim9BallBreak::lastBestCount > 0)
                snprintf(row2, sizeof(row2), "%d balls", Aim9BallBreak::lastBestCount);
            break;
        case AimMode::EIGHTBALL_SAFETY:
            if (!isAimed || !AimSafety::lastHadShot)
                snprintf(row2, sizeof(row2), "No safety found");
            else
                snprintf(row2, sizeof(row2), "Blocked %d  Pkt %d",
                    AimSafety::lastBlockedCount, AimSafety::lastTargetPocket);
            break;
        default: break;
    }

    // ── Draw ke ForegroundDrawList ────────────────────────────────────────
    ImDrawList* fg  = GetForegroundDrawList();
    float       fs  = GImGui->FontSize;
    float       x   = 22.0f;
    float       y   = 28.0f;

    float dotAlpha = isAnalyzing ? 0.90f : 0.60f;
    float dotR     = isAnalyzing ? 4.0f : 3.0f;
    fg->AddCircleFilled(ImVec2(x + 5.0f, y + fs * 0.55f), dotR,
        ImColor(modeColor.x, modeColor.y, modeColor.z, dotAlpha));

    fg->AddText(ImVec2(x + 15.0f + 1, y + 1), IM_COL32(0, 0, 0, 100), row1);
    fg->AddText(ImVec2(x + 15.0f, y),
        ImColor(statusColor.x, statusColor.y, statusColor.z, statusAlpha), row1);
    y += fs + 5.0f;

    if (row2[0] != '\0') {
        fg->AddText(ImVec2(x + 15.0f + 1, y + 1), IM_COL32(0, 0, 0, 80), row2);
        fg->AddText(ImVec2(x + 15.0f, y),
            ImColor(modeColor.x, modeColor.y, modeColor.z, 0.95f), row2);
    }
}

// ── WIN Center Banner — muncul di tengah atas layar ───────────────────────────
static void DrawWinCenterBanner(ImGuiIO& io) {
    if (!AutoAim::bAimed.load()) return;

    bool hasWin = false;
    char winMsg[32] = "";                            // plain buffer — NEVER O() pointer
    ImVec4 winColor = ImVec4(0.10f, 1.0f, 0.45f, 1.0f);

    if (g_aimMode == AimMode::NINEBALL_BREAK && Aim9BallBreak::lastWasWin9) {
        hasWin = true;
        strcpy(winMsg, "WIN 9 FOUND!");
        winColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
    } else if (g_aimMode == AimMode::NINEBALL_PREDICT && Aim9Ball::lastWasCombo) {
        hasWin = true;
        strcpy(winMsg, "COMBO WIN!");
        winColor = ImVec4(0.10f, 1.0f, 0.55f, 1.0f);
    }
    if (!hasWin) return;

    ImDrawList* fg = GetForegroundDrawList();
    float fs2 = GImGui->FontSize * 1.85f;

    float normalW  = GImGui->Font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, winMsg, nullptr, nullptr).x;
    float scaledW  = normalW * 1.85f;
    float cx       = io.DisplaySize.x * 0.5f - scaledW * 0.5f;
    float cy       = 52.0f;

    fg->AddRectFilled(
        ImVec2(cx - 18.0f, cy - 6.0f),
        ImVec2(cx + scaledW + 18.0f, cy + fs2 + 6.0f),
        ImColor(winColor.x * 0.15f, winColor.y * 0.15f, winColor.z * 0.15f, 0.42f),
        10.0f);
    fg->AddRect(
        ImVec2(cx - 18.0f, cy - 6.0f),
        ImVec2(cx + scaledW + 18.0f, cy + fs2 + 6.0f),
        ImColor(winColor.x, winColor.y, winColor.z, 0.45f),
        10.0f, 0, 1.5f);

    fg->AddText(GImGui->Font, fs2, ImVec2(cx + 2, cy + 2), IM_COL32(0, 0, 0, 140), winMsg);
    fg->AddText(GImGui->Font, fs2, ImVec2(cx, cy),
        ImColor(winColor.x, winColor.y, winColor.z, 0.92f), winMsg);
}


static void DrawContentArea(float winW, float winH) {
    bool need_save = false;
    
    ImDrawList* dl  = GetWindowDrawList();
    ImVec2      wp  = GetWindowPos();

    // startY este punctul unde se termină bara de butoane (Sidebar)
    float startY   = GetCursorPosY();
    float contentW = winW;

    // Desenăm fundalul zonei de conținut sub sidebar
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + winH),
        IM_COL32(10, 18, 40, 255), IM_COL32(10, 18, 40, 255),
        IM_COL32(14, 26, 58, 255), IM_COL32(14, 26, 58, 255)
    );
    
    static const char* const tabTitles[] = {
        "Draw",
        "8 Ball",
        "9 Ball",
        "Info"
    };
    // Tab accent colors matching each section
    const ImU32 tabAccents[] = {
        IM_COL32(60, 140, 255, 220),
        IM_COL32(30, 200, 120, 220),
        IM_COL32(200, 165, 30, 220),
        IM_COL32(130, 90, 220, 220),
    };

    const char* currentTitle = tabTitles[g_menu.currentTab];
    ImU32 accentCol = tabAccents[g_menu.currentTab];

    float titlePadT = 14.0f;
    float titlePadB = 10.0f;

    // Title with larger font
    SetWindowFontScale(1.18f);
    ImVec2 ts = CalcTextSize(currentTitle);
    SetWindowFontScale(1.0f);

    float titleBlockH = titlePadT + ts.y + titlePadB;

    // Subtle title background strip
    dl->AddRectFilled(
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + startY + titleBlockH),
        IM_COL32(8, 16, 38, 200));

    // Left accent stripe
    dl->AddRectFilled(
        ImVec2(wp.x, wp.y + startY + titlePadT * 0.5f),
        ImVec2(wp.x + 3.5f, wp.y + startY + titleBlockH - titlePadT * 0.5f),
        accentCol, 2.0f);

    // Title text — draw directly so we can use font scale without affecting cursor flow
    SetWindowFontScale(1.18f);
    float centeredX = (contentW - ts.x) * 0.5f;
    SetCursorPos(ImVec2(centeredX, startY + titlePadT));
    TextColored(ImVec4(0.92f, 0.95f, 1.0f, 1.0f), "%s", currentTitle);
    SetWindowFontScale(1.0f);

    // Separator line with gradient fade on both ends
    float lineY = startY + titleBlockH;
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x,               wp.y + lineY - 0.5f),
        ImVec2(wp.x + contentW * 0.35f, wp.y + lineY + 1.0f),
        IM_COL32(0, 0, 0, 0), accentCol, accentCol, IM_COL32(0, 0, 0, 0));
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + contentW * 0.35f, wp.y + lineY - 0.5f),
        ImVec2(wp.x + contentW,         wp.y + lineY + 1.0f),
        accentCol, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), accentCol);

    float headerH = lineY - startY + 8.0f;
    SetCursorPos(ImVec2(10.0f, startY + headerH));
    
    // Începutul zonei de child (conținutul propriu-zis)
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false);
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 10));

            // ── Enable Play Button (tombol aim di layar game) ─────────────────
            if (ToggleSwitch(O("Enable Play Button"), &persistent_bool[O("bAutoAim")])) {
                AutoAim::bActive = persistent_bool[O("bAutoAim")];
                need_save = true;
            }
            Dummy(ImVec2(0, 14));

            need_save |= ToggleSwitch(O("Draw Lines"), &persistent_bool[O("bESP_DrawPredictionLine")]);
            need_save |= ToggleSwitch(O("Show Pockets"), &persistent_bool[O("bShowPockets")]);
            need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPocketsShotState")]);
            need_save |= ToggleSwitch(O("Enemy Line"), &persistent_bool[O("bESP_EnemyLine")]);

            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Line Thickness"));
            Dummy(ImVec2(0, 8));
            {
                if (persistent_int[O("iLineThickness")] < 1) persistent_int[O("iLineThickness")] = 4;
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.12f, 0.45f, 0.95f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.20f, 0.55f, 1.0f, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt(O("##lineThick"), &persistent_int[O("iLineThickness")], 1, 10, "%d");
                PopStyleColor(3);
                PopStyleVar(2);
            }

            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Fix Menu Size"));
            Dummy(ImVec2(0, 8));
            {
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.12f, 0.45f, 0.95f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.20f, 0.55f, 1.0f, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                int& menuSz = persistent_int[O("iMenuSizeOffset")];
                bool changed = SliderInt(O("##menuSize"), &menuSz, -10, 10,
                    menuSz == 0 ? O("Normal") : "%d");
                need_save |= changed;
                PopStyleColor(3);
                PopStyleVar(2);
            }

            Dummy(ImVec2(0, 20));
            {
                PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
                PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.38f, 0.85f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.48f, 1.00f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f, 0.30f, 0.70f, 1.0f));
                if (Button(O("Save Config"), ImVec2(GetContentRegionAvail().x, 55.0f))) {
                    svConfig_Save();
                }
                PopStyleColor(3);
                PopStyleVar();
            }
            break;
        }
        
        case 1: {
            // ── 8 BALL POOL ───────────────────────────────────────────────
            Dummy(ImVec2(0, 10));

            // ── Mode selector ─────────────────────────────────────────────
            auto ModeSwitch8 = [&](const char* label, AimMode mode) {
                bool isOn  = (g_aimMode == mode);
                bool wasOn = isOn;
                ToggleSwitch(label, &isOn);
                if (isOn && !wasOn) g_aimMode = mode;
                if (!isOn && wasOn) {}
                Dummy(ImVec2(0, 6));
            };

            ModeSwitch8(O("Aim Break"),         AimMode::EIGHTBALL_BREAK);
            ModeSwitch8(O("Aim Predict"),       AimMode::EIGHTBALL_PREDICT);
            ModeSwitch8(O("Aim Lock 8 Ball"),   AimMode::EIGHTBALL_8LOCK);
            ModeSwitch8(O("Aim Lock Pocket"),   AimMode::EIGHTBALL_POCKET_LOCK);
            ModeSwitch8(O("Safety Shot"),       AimMode::EIGHTBALL_SAFETY);

            Dummy(ImVec2(0, 4));

            // ── Pocket Lock Selector Grid ─────────────────────────────────
            if (g_aimMode == AimMode::EIGHTBALL_POCKET_LOCK) {
                Dummy(ImVec2(0, 6));

                float cardW = GetContentRegionAvail().x;
                ImVec2 hdrPos = GetCursorScreenPos();
                ImDrawList* dlpg = GetWindowDrawList();

                // Section header card
                float hdrH = 30.0f;
                dlpg->AddRectFilled(hdrPos, ImVec2(hdrPos.x + cardW, hdrPos.y + hdrH),
                    IM_COL32(10, 20, 48, 230), 10.0f, ImDrawFlags_RoundCornersTop);
                dlpg->AddRect(hdrPos, ImVec2(hdrPos.x + cardW, hdrPos.y + hdrH),
                    IM_COL32(45, 90, 200, 100), 10.0f, ImDrawFlags_RoundCornersTop, 1.0f);

                SetWindowFontScale(0.88f);
                ImVec2 htSz = CalcTextSize(O("Select Pocket to Lock"));
                SetCursorPosX(hdrPos.x - GetWindowPos().x + (cardW - htSz.x) * 0.5f);
                SetCursorPosY(GetCursorPosY() + (hdrH - htSz.y) * 0.5f);
                TextColored(ImVec4(0.65f, 0.75f, 1.0f, 0.95f), "%s", O("Select Pocket to Lock"));
                SetWindowFontScale(1.0f);

                Dummy(ImVec2(0, 6));

                // Pocket grid — 3 columns x 2 rows
                // Visual table layout:
                //  Row 0 (top of table):  [0:TL]  [1:TC]  [2:TR]
                //  Row 1 (bottom):        [5:BL]  [4:BC]  [3:BR]
                const int order[6] = {0, 1, 2, 5, 4, 3};
                const char* pktLabel[6] = {"TL","TC","TR","BR","BC","BL"};
                const char* pktFull[6]  = {"Top-Left","Top-Center","Top-Right","Bot-Right","Bot-Center","Bot-Left"};
                int selPktLock = PocketSelector::Get();

                float gap  = 5.0f;
                float btnW = (cardW - gap * 2.0f) / 3.0f;
                float btnH = 48.0f;
                float t    = (float)GetTime();

                for (int row = 0; row < 2; row++) {
                    for (int col = 0; col < 3; col++) {
                        int idx = order[row * 3 + col];
                        bool isSel = (selPktLock == idx);

                        ImVec2 bmin = GetCursorScreenPos();
                        ImVec2 bmax = ImVec2(bmin.x + btnW, bmin.y + btnH);

                        PushID(idx + 100);
                        bool clicked = InvisibleButton("##pktbtn", ImVec2(btnW, btnH));
                        PopID();

                        if (clicked) {
                            if (isSel) PocketSelector::Reset();
                            else       PocketSelector::selectedPocket.store(idx);
                        }

                        bool hov = IsItemHovered();

                        // Background
                        if (isSel) {
                            float gA = 0.80f + 0.20f * sinf(t * 3.0f);
                            dlpg->AddRectFilledMultiColor(bmin, bmax,
                                IM_COL32(130, 90, 0, 220), IM_COL32(160, 110, 0, 220),
                                IM_COL32(150, 100, 0, 220), IM_COL32(120, 80, 0, 220));
                            dlpg->AddRect(bmin, bmax,
                                ImColor(1.0f, 0.82f, 0.10f, gA), 8.0f, 0, 2.0f);
                        } else if (hov) {
                            dlpg->AddRectFilled(bmin, bmax, IM_COL32(30, 55, 120, 200), 8.0f);
                            dlpg->AddRect(bmin, bmax, IM_COL32(70, 120, 220, 180), 8.0f, 0, 1.2f);
                        } else {
                            dlpg->AddRectFilled(bmin, bmax, IM_COL32(10, 18, 42, 200), 8.0f);
                            dlpg->AddRect(bmin, bmax, IM_COL32(35, 55, 100, 150), 8.0f, 0, 1.0f);
                        }

                        // Pocket number circle
                        float cx = bmin.x + btnW * 0.5f;
                        float cy = bmin.y + btnH * 0.5f - 5.0f;
                        float cr = 10.0f;
                        dlpg->AddCircleFilled(ImVec2(cx, cy), cr,
                            isSel ? IM_COL32(255, 210, 30, 180) : IM_COL32(40, 70, 140, 180));
                        char numStr[4];
                        snprintf(numStr, sizeof(numStr), "%d", idx);
                        ImVec2 ns = GImGui->Font->CalcTextSizeA(GImGui->FontSize * 0.82f, FLT_MAX, 0.f, numStr);
                        dlpg->AddText(GImGui->Font, GImGui->FontSize * 0.82f,
                            ImVec2(cx - ns.x * 0.5f, cy - ns.y * 0.5f),
                            isSel ? IM_COL32(20, 10, 0, 255) : IM_COL32(200, 220, 255, 220),
                            numStr);

                        // Short label below circle
                        ImVec2 lsz = GImGui->Font->CalcTextSizeA(GImGui->FontSize * 0.78f, FLT_MAX, 0.f, pktLabel[idx]);
                        dlpg->AddText(GImGui->Font, GImGui->FontSize * 0.78f,
                            ImVec2(bmin.x + (btnW - lsz.x) * 0.5f, bmin.y + btnH - lsz.y - 5.0f),
                            isSel ? IM_COL32(255, 240, 160, 255) : IM_COL32(130, 150, 195, 200),
                            pktLabel[idx]);

                        if (col < 2) { SameLine(0, gap); }
                    }
                    if (row < 1) Dummy(ImVec2(0, gap));
                }

                Dummy(ImVec2(0, 6));

                // Status strip
                if (selPktLock >= 0) {
                    float sW = cardW;
                    ImVec2 sPos = GetCursorScreenPos();
                    float sH = 36.0f;
                    dlpg->AddRectFilled(sPos, ImVec2(sPos.x + sW, sPos.y + sH),
                        IM_COL32(60, 40, 0, 210), 8.0f);
                    dlpg->AddRect(sPos, ImVec2(sPos.x + sW, sPos.y + sH),
                        IM_COL32(220, 175, 20, 180), 8.0f, 0, 1.2f);

                    // Lock icon (small diamond)
                    float lx = sPos.x + 14.0f;
                    float ly = sPos.y + sH * 0.5f;
                    dlpg->AddCircleFilled(ImVec2(lx, ly), 4.5f, IM_COL32(255, 205, 20, 230));

                    SetCursorPosY(GetCursorPosY() + (sH - GImGui->FontSize) * 0.5f);
                    SetCursorPosX(GetCursorPosX() + 24.0f);
                    char lockBuf[48];
                    snprintf(lockBuf, sizeof(lockBuf), "Locked  [%d] %s", selPktLock, pktFull[selPktLock]);
                    SetWindowFontScale(0.90f);
                    TextColored(ImVec4(1.0f, 0.88f, 0.20f, 1.0f), "%s", lockBuf);
                    SetWindowFontScale(1.0f);
                    Dummy(ImVec2(0, sH - (GImGui->FontSize + (sH - GImGui->FontSize) * 0.5f)));

                    Dummy(ImVec2(0, 4));
                    PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.40f, 0.10f, 0.05f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.15f, 0.08f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.30f, 0.08f, 0.04f, 1.0f));
                    if (Button(O("Reset (Auto)"), ImVec2(cardW, 34.0f)))
                        PocketSelector::Reset();
                    PopStyleColor(3);
                    PopStyleVar();
                } else {
                    SetWindowFontScale(0.85f);
                    TextColored(ImVec4(0.40f, 0.48f, 0.65f, 0.85f),
                        O("  Tap a pocket above, then press Aim"));
                    SetWindowFontScale(1.0f);
                }
                Dummy(ImVec2(0, 8));
            }

            // ── Pocket Info (read-only, selalu otomatis) ──────────────────
            if (g_aimMode == AimMode::EIGHTBALL_PREDICT || g_aimMode == AimMode::EIGHTBALL_8LOCK) {
                Dummy(ImVec2(0, 14));

                float cardW = GetContentRegionAvail().x;
                ImVec2 cPos = GetCursorScreenPos();
                ImDrawList* dlp = GetWindowDrawList();

                // Tentukan pocket terakhir yang dipilih engine
                int lastPkt = -1;
                if (g_aimMode == AimMode::EIGHTBALL_PREDICT)
                    lastPkt = AimLockTarget::lastTargetPocket;
                else
                    lastPkt = AimLock8Ball::lastTargetPocket;

                bool hasResult = (AutoAim::bAimed.load() && lastPkt >= 0);

                // Background pill
                float cardH = 48.0f;
                dlp->AddRectFilled(cPos, ImVec2(cPos.x + cardW, cPos.y + cardH),
                    IM_COL32(14, 22, 40, 200), 10.0f);

                // Label kiri — "Pocket"
                SetCursorPosY(GetCursorPosY() + 12.0f);
                SetCursorPosX(GetCursorPosX() + 14.0f);
                TextColored(ImVec4(0.45f, 0.50f, 0.65f, 1.0f), "Pocket");

                SameLine();

                // Nilai pocket di kanan
                if (hasResult) {
                    char pBuf[16];
                    snprintf(pBuf, sizeof(pBuf), "P%d", lastPkt);
                    // Pill hijau kecil di belakang angka
                    ImVec2 vp = GetCursorScreenPos();
                    ImVec2 ts = CalcTextSize(pBuf);
                    float pad = 8.0f;
                    float t   = (float)GetTime();
                    float gA  = 0.55f + 0.30f * sinf(t * 3.0f);
                    dlp->AddRectFilled(
                        ImVec2(vp.x - pad, vp.y - 3),
                        ImVec2(vp.x + ts.x + pad, vp.y + ts.y + 3),
                        IM_COL32(8, 50, 25, 200), 6.0f);
                    dlp->AddRect(
                        ImVec2(vp.x - pad, vp.y - 3),
                        ImVec2(vp.x + ts.x + pad, vp.y + ts.y + 3),
                        ImColor(0.05f, 0.90f, 0.42f, gA), 6.0f, 0, 1.2f);
                    TextColored(ImVec4(0.10f, 0.95f, 0.50f, 1.0f), "%s", pBuf);
                    SameLine(0, 12.0f);
                    // Label: user-selected vs auto
                    int selPkt = PocketSelector::Get();
                    if (selPkt >= 0) {
                        TextColored(ImVec4(1.0f, 0.85f, 0.15f, 0.95f), "(locked)");
                    } else {
                        TextColored(ImVec4(0.35f, 0.40f, 0.50f, 0.80f), "(auto)");
                    }
                } else if (g_autoPlayCalculating.load() || g_aimThreadRunning.load()) {
                    TextColored(ImVec4(1.0f, 0.75f, 0.0f, 0.90f), "scanning...");
                } else {
                    TextColored(ImVec4(0.35f, 0.40f, 0.55f, 0.70f), "-- (press aim)");
                }

                Dummy(ImVec2(0, cardH - 24.0f));
            }

            // Pocket Selector info + reset button
            {
                int selPkt = PocketSelector::Get();
                Dummy(ImVec2(0, 10));

                float infoW = GetContentRegionAvail().x;
                ImVec2 infoPos = GetCursorScreenPos();
                ImDrawList* dlp2 = GetWindowDrawList();

                // Background card
                float infoH = selPkt >= 0 ? 70.0f : 72.0f;
                dlp2->AddRectFilled(infoPos, ImVec2(infoPos.x + infoW, infoPos.y + infoH),
                    IM_COL32(12, 18, 32, 210), 10.0f);
                dlp2->AddRect(infoPos, ImVec2(infoPos.x + infoW, infoPos.y + infoH),
                    selPkt >= 0 ? IM_COL32(230, 180, 20, 180) : IM_COL32(50, 60, 90, 160),
                    10.0f, 0, 1.2f);

                SetCursorPosY(GetCursorPosY() + 8.0f);
                SetCursorPosX(GetCursorPosX() + 12.0f);

                if (selPkt >= 0) {
                    TextColored(ImVec4(1.0f, 0.85f, 0.15f, 1.0f), "Pocket Terpilih:");
                    SameLine();
                    const char* pNames[6] = {"Top-Left","Top-Center","Top-Right","Bot-Right","Bot-Center","Bot-Left"};
                    TextColored(ImVec4(0.15f, 1.0f, 0.55f, 1.0f), " [%d] %s", selPkt, pNames[selPkt]);

                    SetCursorPosX(GetCursorPosX() + 12.0f);
                    PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.15f, 0.08f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.10f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.10f, 0.05f, 1.0f));
                    if (Button(O("Reset Auto"), ImVec2(infoW - 24.0f, 26.0f)))
                        PocketSelector::Reset();
                    PopStyleColor(3);
                    PopStyleVar();
                } else {
                    // Ambil lastTargetPocket dari mode yang aktif
                    int lastPkt = -1;
                    if (g_aimMode == AimMode::EIGHTBALL_PREDICT)
                        lastPkt = AimLockTarget::lastTargetPocket;
                    else if (g_aimMode == AimMode::EIGHTBALL_8LOCK)
                        lastPkt = AimLock8Ball::lastTargetPocket;

                    if (lastPkt >= 0 && !g_aimThreadRunning.load()) {
                        // Ada hasil aim sebelumnya — tampilkan tombol Lock
                        const char* pNamesL[6] = {"Top-L","Top-C","Top-R","Bot-R","Bot-C","Bot-L"};
                        TextColored(ImVec4(0.55f, 0.65f, 0.90f, 1.0f),
                            "Saran: [%d] %s", lastPkt, pNamesL[lastPkt]);

                        SetCursorPosX(GetCursorPosX() + 12.0f);
                        PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                        PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.35f, 0.55f, 1.0f));
                        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.50f, 0.78f, 1.0f));
                        PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f, 0.28f, 0.44f, 1.0f));
                        char lockLabel[40];
                        snprintf(lockLabel, sizeof(lockLabel),
                            "Kunci ke [%d] %s", lastPkt, pNamesL[lastPkt]);
                        if (Button(lockLabel, ImVec2(infoW - 24.0f, 26.0f)))
                            PocketSelector::selectedPocket.store(lastPkt);
                        PopStyleColor(3);
                        PopStyleVar();
                    } else {
                        TextColored(ImVec4(0.45f, 0.55f, 0.70f, 1.0f),
                            "Jalankan Auto Aim dahulu");
                        SetCursorPosX(GetCursorPosX() + 12.0f);
                        TextColored(ImVec4(0.30f, 0.38f, 0.55f, 0.80f),
                            "Lalu klik Lock untuk mengunci pocket");
                    }
                }
                // Tinggi card menyesuaikan konten
                float usedH = selPkt >= 0 ? 46.0f : 46.0f;
                float leftH = infoH - usedH;
                if (leftH > 0) Dummy(ImVec2(0, leftH));
            }

            break;
        }

        case 2: {
            // ── 9 BALL POOL ───────────────────────────────────────────────
            Dummy(ImVec2(0, 10));

            // ── Mode selector: radio-style toggle switches ─────────────────
            auto ModeSwitch9 = [&](const char* label, AimMode mode) {
                bool isOn  = (g_aimMode == mode);
                bool wasOn = isOn;
                ToggleSwitch(label, &isOn);
                if (isOn && !wasOn) g_aimMode = mode;
                if (!isOn && wasOn) {}
                Dummy(ImVec2(0, 8));
            };

            ModeSwitch9(O("Aim Ghost 90% Win"), AimMode::NINEBALL_BREAK);
            ModeSwitch9(O("Aim Predict"),       AimMode::NINEBALL_PREDICT);

            Dummy(ImVec2(0, 4));

            break;
        }
        
        case 3: {
            // ── INFO ──────────────────────────────────────────────────────────
            ImDrawList* dl3 = GetWindowDrawList();

            // Helper: section header dengan gradient line kiri-kanan
            auto DrawSecHdr = [&](const char* title, ImVec4 lineCol) {
                Dummy(ImVec2(0, 14));
                float avail = GetContentRegionAvail().x;
                ImVec2 p    = GetCursorScreenPos();
                float  fs   = GImGui->FontSize;
                ImVec2 ts   = CalcTextSize(title);
                float  gap  = 10.0f;
                float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
                float  lineY = p.y + fs * 0.52f;
                ImU32  lc   = ImColor(lineCol);
                ImU32  lc0  = ImColor(lineCol.x, lineCol.y, lineCol.z, 0.0f);
                // Gradient line kiri (fade ke kanan)
                dl3->AddRectFilledMultiColor(
                    ImVec2(p.x, lineY - 1.0f), ImVec2(p.x + lineW, lineY + 1.5f),
                    lc0, lc, lc, lc0);
                // Gradient line kanan (fade ke kiri)
                dl3->AddRectFilledMultiColor(
                    ImVec2(p.x + lineW + gap + ts.x + gap, lineY - 1.0f),
                    ImVec2(p.x + avail, lineY + 1.5f),
                    lc, lc0, lc0, lc);
                SetCursorPosX(GetCursorPosX() + lineW + gap);
                TextColored(lineCol, "%s", title);
                Dummy(ImVec2(0, 8));
            };

            // Helper: baris info — key kiri, value kanan (aligned)
            auto DrawInfoRow = [&](const char* key, const char* val, ImVec4 valColor) {
                ImVec2 cp = GetCursorScreenPos();
                float  fs = GImGui->FontSize;
                // Dot kecil dekorasi
                dl3->AddCircleFilled(ImVec2(cp.x + 5.0f, cp.y + fs * 0.52f),
                    2.5f, IM_COL32(60, 130, 220, 180));
                SetCursorPosX(GetCursorPosX() + 14.0f);
                TextColored(ImVec4(0.45f, 0.50f, 0.62f, 1.0f), "%s", key);
                SameLine();
                TextColored(valColor, "%s", val);
                Dummy(ImVec2(0, 4));
            };

            // ── Kartu @LYN4XP (hero card) ─────────────────────────────────────
            {
                static const char* NAME_STR = "@LYN4XP";
                static const char* LINK_STR = "t.me/Lyn4xp";
                float cardW = GetContentRegionAvail().x;
                float cardH = 88.0f;
                ImVec2 cPos = GetCursorScreenPos();

                // Background gelap gradient
                dl3->AddRectFilledMultiColor(
                    cPos, ImVec2(cPos.x + cardW, cPos.y + cardH),
                    IM_COL32(5, 20, 12, 240), IM_COL32(8, 28, 18, 240),
                    IM_COL32(6, 24, 14, 240), IM_COL32(4, 18, 10, 240));
                // Border hijau neon static
                dl3->AddRect(cPos, ImVec2(cPos.x + cardW, cPos.y + cardH),
                    IM_COL32(12, 240, 115, 200), 14.0f, 0, 1.8f);

                // @LYN4XP — centered, font besar, pakai AddText langsung (tidak manipulasi cursor)
                float bigFs = GImGui->FontSize * 1.55f;
                float nameW = GImGui->Font->CalcTextSizeA(bigFs, FLT_MAX, 0.0f, NAME_STR, nullptr, nullptr).x;
                float nameX = cPos.x + (cardW - nameW) * 0.5f;
                float nameY = cPos.y + 13.0f;
                dl3->AddText(GImGui->Font, bigFs,
                    ImVec2(nameX + 2.0f, nameY + 2.0f), IM_COL32(0, 60, 20, 140), NAME_STR);
                dl3->AddText(GImGui->Font, bigFs,
                    ImVec2(nameX, nameY), IM_COL32(20, 242, 115, 255), NAME_STR);

                // t.me/Lyn4xp — centered, font normal
                float linkW = GImGui->Font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, LINK_STR, nullptr, nullptr).x;
                float linkX = cPos.x + (cardW - linkW) * 0.5f;
                float linkY = nameY + bigFs + 5.0f;
                dl3->AddText(GImGui->Font, GImGui->FontSize,
                    ImVec2(linkX, linkY), IM_COL32(64, 192, 115, 204), LINK_STR);

                // Tanda Telegram kecil di pojok kiri bawah
                static const char* TG_LABEL = "Telegram";
                float tgY = cPos.y + cardH - GImGui->FontSize - 7.0f;
                dl3->AddText(GImGui->Font, GImGui->FontSize * 0.82f,
                    ImVec2(cPos.x + 10.0f, tgY), IM_COL32(64, 148, 220, 170), TG_LABEL);

                // Reserve ruang card dengan Dummy — tidak ada cursor manipulation
                Dummy(ImVec2(cardW, cardH + 6.0f));
            }

            // ── Mod Info ─────────────────────────────────────────────────────
            DrawSecHdr("Mod Info", ImVec4(0.30f, 0.65f, 1.0f, 0.85f));
            {
                static const char* ENGINE_VAL = "Flux Pro Engine v2.0";
                static const char* GAME_VAL   = "8 Ball Pool";
                static const char* ARCH_VAL   = "arm64-v8a";
                static const char* BUILD_VAL  = __DATE__ " " __TIME__;
                DrawInfoRow("Engine",  ENGINE_VAL, ImVec4(0.88f, 0.92f, 1.0f, 1.0f));
                DrawInfoRow("Game",    GAME_VAL,   ImVec4(0.88f, 0.92f, 1.0f, 1.0f));
                DrawInfoRow("Arch",    ARCH_VAL,   ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
                DrawInfoRow("Build",   BUILD_VAL,  ImVec4(0.65f, 0.70f, 0.80f, 1.0f));

                int64_t now_ts = (int64_t)time(nullptr);
                int64_t diff   = EXPIRY_TS - now_ts;
                char expBuf[64];
                if (diff > 0) {
                    int days  = (int)(diff / 86400);
                    int hours = (int)((diff % 86400) / 3600);
                    int mins  = (int)((diff % 3600)  / 60);
                    snprintf(expBuf, sizeof(expBuf), "%dd %dh %dm", days, hours, mins);
                    DrawInfoRow("Expires", expBuf, ImVec4(0.30f, 1.0f, 0.55f, 1.0f));
                } else {
                    DrawInfoRow("Expires", "EXPIRED", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
                }
            }

            // ── Contact ───────────────────────────────────────────────────────
            DrawSecHdr("Contact", ImVec4(0.35f, 0.75f, 1.0f, 0.85f));
            {
                static const char* TG_USER = "@LYN4XP";
                static const char* TG_LINK = "t.me/Lyn4xp";
                DrawInfoRow("Telegram", TG_USER, ImVec4(0.40f, 0.82f, 1.0f, 1.0f));
                DrawInfoRow("Channel",  TG_LINK, ImVec4(0.28f, 0.70f, 0.95f, 1.0f));
                DrawInfoRow("Updates",  TG_LINK, ImVec4(0.55f, 0.85f, 1.0f, 0.85f));
            }

            // ── Device Info ───────────────────────────────────────────────────
            DrawSecHdr("Device", ImVec4(0.25f, 0.80f, 0.55f, 0.85f));
            {
                static char s_mfr[PROP_VALUE_MAX] = {};
                static char s_mdl[PROP_VALUE_MAX] = {};
                static char s_abi[PROP_VALUE_MAX] = {};
                static char s_and[PROP_VALUE_MAX] = {};
                static bool loaded = false;
                if (!loaded) {
                    __system_property_get("ro.product.manufacturer", s_mfr);
                    __system_property_get("ro.product.model",        s_mdl);
                    __system_property_get("ro.product.cpu.abi",      s_abi);
                    __system_property_get("ro.build.version.release",s_and);
                    loaded = true;
                }
                DrawInfoRow("Maker",   s_mfr, ImVec4(0.88f, 0.92f, 1.0f, 1.0f));
                DrawInfoRow("Model",   s_mdl, ImVec4(0.88f, 0.92f, 1.0f, 1.0f));
                DrawInfoRow("ABI",     s_abi, ImVec4(0.65f, 0.70f, 0.80f, 1.0f));
                DrawInfoRow("Android", s_and, ImVec4(0.65f, 0.70f, 0.80f, 1.0f));
            }

            // ── Free Beta notice ──────────────────────────────────────────────
            Dummy(ImVec2(0, 10));
            {
                static const char* BETA_MSG = "FREE BETA — Join t.me/Lyn4xp for updates";
                float nW = GetContentRegionAvail().x;
                ImVec2 nPos = GetCursorScreenPos();
                float  nH   = GImGui->FontSize + 16.0f;
                dl3->AddRectFilled(nPos, ImVec2(nPos.x + nW, nPos.y + nH),
                    IM_COL32(8, 35, 55, 220), 8.0f);
                dl3->AddRect(nPos, ImVec2(nPos.x + nW, nPos.y + nH),
                    IM_COL32(40, 140, 220, 120), 8.0f, 0, 1.0f);
                Dummy(ImVec2(0, 5));
                SetCursorPosX(GetCursorPosX() + 10.0f);
                PushTextWrapPos(GetCursorPosX() + nW - 20.0f);
                TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "%s", BETA_MSG);
                PopTextWrapPos();
                Dummy(ImVec2(0, 6));
            }
            break;
        }

    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleColor();
}

INLINE void DrawMenu(ImGuiIO& io) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        // Auto-open menu saat pertama kali bypass login
        if (DEBUG_BYPASS_LOGIN) {
            static bool s_autoOpened = false;
            if (!s_autoOpened) { g_menu.isOpen = true; s_autoOpened = true; }
        }

        // Restore state dari persistent storage saat pertama kali jalan
        {
            static bool s_stateRestored = false;
            if (!s_stateRestored) {
                AutoAim::bActive = persistent_bool[O("bAutoAim")];
                s_stateRestored  = true;
            }
        }

        // Auto-open menu ketika match dimulai (transisi tidak-ingame → ingame)
        {
            static bool s_wasInGame = false;
            if (g_espIsInGame && !s_wasInGame) {
                g_menu.isOpen = true;
            }
            s_wasInGame = g_espIsInGame;
        }

        buttonClicker.Update();
        AutoAim::Update();
        PocketSelector::Update();

        g_espStateReady = false;
        g_espIsInGame   = false;    // reset tiap frame — DrawESP akan set true jika memang in-game

        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        if (g_espStateReady && persistent_bool[O("bAutoAim")] && g_espIsInGame) {
            DrawToggleButton();
        }

        float targetAlpha = g_menu.isOpen ? 1.0f : 0.0f;
        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarWidth * sizeScale;
            float winH = 560.0f * sizeScale;
            
            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.13f, 0.f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 32.0f);
            PushStyleVar(ImGuiStyleVar_ChildRounding,  22.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);
            
            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            
            if (Begin(O("##MainMenu"), &g_menu.isOpen, winFlags)) {
                DrawSidebar(winW);
                DrawContentArea(winW, winH);
            }
            End();
            
            PopStyleVar(5);
            PopStyleColor();
        }
    }
}

// Toggle button shown in-game — tap once to enable/disable AutoAim (aim stays on until tapped again)
static void DrawToggleButton() {
    ImGuiIO& io = GetIO();

    float button_size   = 88.0f;
    float winPadX       = GetStyle().WindowPadding.x;
    float winPadY       = GetStyle().WindowPadding.y;
    float windowWidth   = button_size + winPadX * 2.0f;
    float windowHeight  = button_size + winPadY * 2.0f;

    const float rightMargin  = 20.0f;
    float fixedX = io.DisplaySize.x - rightMargin - windowWidth;

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 20.0f - windowHeight;

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 99.0f);

    if (Begin(O("##ToggleBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {

        ImVec2 pos    = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

        bool clicked = InvisibleButton(O("##TglBtnHit"), size);

        bool aimed = AutoAim::bAimed;
        bool calc  = g_autoPlayCalculating;

        float r  = button_size * 0.5f;
        float r2 = r - 3.0f;
        ImDrawList* dl = GetWindowDrawList();

        // Shadow
        dl->AddCircleFilled(center, r + 5.0f, IM_COL32(0, 0, 0, 65), 56);

        // Background fill
        if (aimed) {
            // Aimed/lock: deep blue-green
            dl->AddCircleFilled(center, r, IM_COL32(10, 48, 110, 248), 56);
            // Outer ring glow
            dl->AddCircle(center, r, IM_COL32(50, 140, 255, 180), 56, 2.4f);
            dl->AddCircle(center, r + 3.0f, IM_COL32(30, 100, 220, 80), 56, 2.8f);
            // Inner concentric rings (target style)
            dl->AddCircle(center, r2 * 0.65f, IM_COL32(80, 180, 255, 130), 40, 1.4f);
            dl->AddCircle(center, r2 * 0.35f, IM_COL32(120, 210, 255, 160), 32, 1.2f);
            // Center dot
            dl->AddCircleFilled(center, r2 * 0.12f, IM_COL32(180, 230, 255, 230));
            // Crosshair lines
            float cg = r2 * 0.38f;
            ImU32 cc = IM_COL32(140, 210, 255, 180);
            dl->AddLine(ImVec2(center.x - r2, center.y), ImVec2(center.x - cg, center.y), cc, 1.4f);
            dl->AddLine(ImVec2(center.x + cg, center.y), ImVec2(center.x + r2, center.y), cc, 1.4f);
            dl->AddLine(ImVec2(center.x, center.y - r2), ImVec2(center.x, center.y - cg), cc, 1.4f);
            dl->AddLine(ImVec2(center.x, center.y + cg), ImVec2(center.x, center.y + r2), cc, 1.4f);
        } else if (calc) {
            // Calculating: amber/yellow
            dl->AddCircleFilled(center, r, IM_COL32(60, 35, 5, 245), 56);
            dl->AddCircle(center, r, IM_COL32(230, 160, 30, 200), 56, 2.2f);
            dl->AddCircle(center, r + 3.0f, IM_COL32(200, 130, 20, 70), 56, 2.6f);
            // Spinning dashes indicator
            float ang0 = (float)GetTime() * 4.0f;
            for (int i = 0; i < 8; i++) {
                float a = ang0 + i * 3.14159f * 2.0f / 8.0f;
                float ri = r2 * 0.72f, ro = r2 * 0.92f;
                float alpha = (i < 4) ? (float)(i + 1) / 5.0f : (float)(8 - i) / 5.0f;
                dl->AddLine(
                    ImVec2(center.x + cosf(a) * ri, center.y + sinf(a) * ri),
                    ImVec2(center.x + cosf(a) * ro, center.y + sinf(a) * ro),
                    IM_COL32(240, 175, 40, (int)(alpha * 220)), 2.0f);
            }
            dl->AddCircleFilled(center, r2 * 0.18f, IM_COL32(240, 175, 40, 200));
        } else {
            // Idle/ready: dark with play arrow
            dl->AddCircleFilled(center, r, IM_COL32(12, 18, 38, 240), 56);
            dl->AddCircle(center, r, IM_COL32(60, 90, 170, 150), 56, 2.0f);
            dl->AddCircle(center, r + 3.0f, IM_COL32(40, 70, 150, 55), 56, 2.4f);
            // Play triangle (▶)
            float tp = r2 * 0.72f;
            float ta = r2 * 0.50f;
            ImVec2 t1(center.x - ta * 0.60f, center.y - tp * 0.62f);
            ImVec2 t2(center.x - ta * 0.60f, center.y + tp * 0.62f);
            ImVec2 t3(center.x + ta * 1.0f,  center.y);
            dl->AddTriangleFilled(t1, t2, t3, IM_COL32(130, 165, 230, 210));
        }

        // Vertical-only drag
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - windowHeight);
            SetWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);
        }

        if (clicked && !g_aimThreadRunning.load()) {
            // Reset hasil aim sebelumnya agar overlay bersih
            AimLockTarget::lastTargetBall  = -1; AimLockTarget::lastHadShot    = false;
            AimLock8Ball::lastHadShot      = false; AimLock8Ball::lastTargetPocket = -1;
            AimBreak::lastBestCount        = 0;  AimBreak::lastTargetBall       = -1;
            Aim9Ball::lastTargetBall       = -1; Aim9Ball::lastWasCombo         = false;
            Aim9BallBreak::lastWasWin9     = false; Aim9BallBreak::lastBestCount = 0;
            AimSafety::lastHadShot         = false; AimSafety::lastBlockedCount  = 0;

            g_autoPlayCalculating = true;
            AutoAim::bAimed       = false;
            g_aimThreadRunning    = true;

            AimMode capturedMode = g_aimMode;
            std::thread([capturedMode]() {
                switch (capturedMode) {
                    case AimMode::EIGHTBALL_PREDICT:
                        AimLockTarget::Aim();
                        break;
                    case AimMode::EIGHTBALL_BREAK:
                        AimBreak::Aim();
                        // Break shot: tidak perlu tap pocket
                        break;
                    case AimMode::EIGHTBALL_8LOCK:
                        AimLock8Ball::Aim();
                        break;
                    case AimMode::EIGHTBALL_POCKET_LOCK:
                        AimLockTarget::Aim();
                        break;
                    case AimMode::NINEBALL_PREDICT:
                        Aim9Ball::Aim();
                        // 9-ball tidak wajib pocket nomination
                        break;
                    case AimMode::NINEBALL_BREAK:
                        Aim9BallBreak::Aim();
                        break;
                    case AimMode::EIGHTBALL_SAFETY:
                        AimSafety::Aim();
                        break;
                    default:
                        AutoAim::TriggerAim();
                        break;
                }
                AutoAim::bAimed       = true;
                g_autoPlayCalculating = false;
                g_aimThreadRunning    = false;
            }).detach();
        }
    }
    End();
    PopStyleVar();
    PopStyleColor(2);
}

static void DrawFloatingButton(ImGuiIO& io) {
    static bool isDragging = false;

    float buttonRadius = 34.0f;
    float buttonSize   = buttonRadius * 2.0f;
    float winSize      = buttonSize + 10.0f;
    float margin       = 8.0f;

    float toggleWinH = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.y * 2.0f;
    const float rightMargin = 20.0f;
    float toggleWinW = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.x * 2.0f;
    float fixedX = io.DisplaySize.x - rightMargin - ImMax(winSize, toggleWinW);

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 80.0f - toggleWinH;

    float posY = g_sideBtnsY - winSize - margin;

    SetNextWindowPos(ImVec2(fixedX, posY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(winSize, winSize), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 center  = ImVec2(fixedX + buttonRadius + 5.0f, posY + buttonRadius + 5.0f);

        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));

        bool menuOpen = g_menu.isOpen;

        // Outer shadow ring
        dl->AddCircleFilled(center, buttonRadius + 6.0f, IM_COL32(0, 0, 0, 70), 48);

        // Background circle — glass-dark
        dl->AddCircleFilled(center, buttonRadius,
            menuOpen ? IM_COL32(18, 55, 130, 242) : IM_COL32(14, 20, 42, 235), 48);

        // Rim highlight arc (top-left gloss)
        dl->AddCircle(center, buttonRadius - 1.5f,
            menuOpen ? IM_COL32(80, 160, 255, 90) : IM_COL32(60, 100, 200, 70), 48, 1.5f);
        dl->AddCircle(center, buttonRadius,
            menuOpen ? IM_COL32(50, 130, 255, 160) : IM_COL32(40, 80, 180, 120), 48, 1.8f);

        float ic = buttonRadius * 0.38f;

        if (menuOpen) {
            // X icon when menu is open
            ImU32 xc = IM_COL32(200, 220, 255, 230);
            dl->AddLine(ImVec2(center.x - ic, center.y - ic),
                        ImVec2(center.x + ic, center.y + ic), xc, 2.6f);
            dl->AddLine(ImVec2(center.x + ic, center.y - ic),
                        ImVec2(center.x - ic, center.y + ic), xc, 2.6f);
        } else {
            // Hamburger icon (3 lines) when closed
            ImU32 hc = IM_COL32(180, 200, 240, 210);
            float lw = ic * 1.55f;
            float ls = ic * 0.60f;
            dl->AddLine(ImVec2(center.x - lw, center.y - ls),
                        ImVec2(center.x + lw, center.y - ls), hc, 2.4f);
            dl->AddLine(ImVec2(center.x - lw, center.y),
                        ImVec2(center.x + lw, center.y), hc, 2.4f);
            dl->AddLine(ImVec2(center.x - lw, center.y + ls),
                        ImVec2(center.x + lw, center.y + ls), hc, 2.4f);
        }

        // Drag (vertical only)
        if (IsItemActive() && IsMouseDragging(0)) {
            isDragging = true;
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, winSize + margin,
                                  io.DisplaySize.y - 80.0f - toggleWinH);
        }

        if (IsItemHovered() && IsMouseReleased(0) && !isDragging)
            g_menu.isOpen = !g_menu.isOpen;

        if (!IsItemActive()) isDragging = false;
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}


static bool first_time = true;
INLINE void DrawLogin(ImGuiIO& io) {
    if (logged_in) return DrawMenu(io);

    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 0.96f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = 580;
    float cardH = 420;

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.14f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    DrawGradientRect(dl, winPos, ImVec2(winPos.x + cardW, winPos.y + 110), IM_COL32(35, 95, 170, 255), IM_COL32(55, 125, 200, 255), true);
    dl->AddRectFilled(winPos, ImVec2(winPos.x + cardW, winPos.y + 20), IM_COL32(35, 95, 170, 255), 20.0f, ImDrawFlags_RoundCornersTop);

    SetWindowFontScale(1.4f);
    ImVec2 titleSize = CalcTextSize(O("@LYN4XP"));
    dl->AddText(ImVec2(winPos.x + (cardW - titleSize.x) * 0.5f, winPos.y + 30), IM_COL32(255, 255, 255, 255), O("@LYN4XP"));
    SetWindowFontScale(1.0f);
    
    ImVec2 subSize = CalcTextSize(O("t.me/Lyn4xp"));
    dl->AddText(ImVec2(winPos.x + (cardW - subSize.x) * 0.5f, winPos.y + 70), IM_COL32(200, 220, 255, 200), O("t.me/Lyn4xp"));

    SetCursorPosY(130);

    if (!ERROR_MESSAGE.empty()) {
        SetCursorPosX(30);
        PushTextWrapPos(cardW - 30);
        TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ERROR_MESSAGE.c_str());
        PopTextWrapPos();
        Dummy(ImVec2(0, 15));
    }

    if (is_logging_in) {
        SetCursorPosY(180);
        
        static float spinner_angle = 0.0f;
        spinner_angle += io.DeltaTime * 5.0f;

        float spinner_size = 40.0f;
        ImVec2 spinnerCenter = ImVec2(winPos.x + cardW * 0.5f, winPos.y + 220);

        for (int i = 0; i < 12; i++) {
            float angle = spinner_angle + (i * PI * 2.0f / 12.0f);
            float alpha = (float)(12 - i) / 12.0f;
            ImVec2 dotPos = ImVec2(
                spinnerCenter.x + cosf(angle) * spinner_size,
                spinnerCenter.y + sinf(angle) * spinner_size
            );
            dl->AddCircleFilled(dotPos, 6.0f, IM_COL32(100, 180, 255, (int)(alpha * 255)));
        }

        ImVec2 loadingSize = CalcTextSize(O("Authenticating..."));
        SetCursorPosX((cardW - loadingSize.x) * 0.5f);
        SetCursorPosY(290);
        TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("Authenticating..."));
    } else {
        SetCursorPosY(160);
        
        ImVec2 infoSize = CalcTextSize(O("Copy your license key and tap login"));
        SetCursorPosX((cardW - infoSize.x) * 0.5f);
        TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.0f), O("Copy your license key and tap login"));
        
        Dummy(ImVec2(0, 50));
        
        bool AutoLogin = first_time && !persistent_string["key"].empty();
        
        SetCursorPosX(40);
        PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.58f, 0.90f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.45f, 0.72f, 1.0f));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        
     if (AutoLogin || Button("LGNBTN", ImVec2(cardW - 80, 65))) {
    if (DEBUG_BYPASS_LOGIN) {
        // Debug bypass: open menu immediately
        logged_in = true;
        g_menu.isOpen = true;
    } else {
        JNIEnv* env;
        jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (getEnvResult == JNI_EDETACHED) {
            if (VM->AttachCurrentThread(&env, nullptr) != 0) ERROR_MESSAGE = O("Failed to attach thread to JVM");
        } else if (getEnvResult != JNI_OK) {
            ERROR_MESSAGE = O("Failed to get JNIEnv");
        } else {
            std::thread([](std::string androidId, std::string key) {
                Login(androidId, key);
            }, getAndroidID(env), AutoLogin ? persistent_string["key"] : getClipboard(env)).detach();
        }
        first_time = false;
    }
}
        
        PopStyleVar();
        PopStyleColor(3);
        
        Dummy(ImVec2(0, 35));
        
        ImVec2 helpSize = CalcTextSize(O("Your will read"));
        SetCursorPosX((cardW - helpSize.x) * 0.5f);
        TextColored(ImVec4(0.42f, 0.42f, 0.48f, 1.0f), O("Your will read"));
    }

    End();
    PopStyleVar(3);
    PopStyleColor();
    
    End();
}


INLINE void SetupImgui() {
    PACKAGE_NAME = string(getcmdline());

    ImGui::CreateContext();

    auto& style = ImGui::GetStyle();
    auto& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;

    switch_theme(current_theme);

    load_persistence();
    svConfig_Load();
    load_imgui_style();

    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    ImFontConfig font_cfg;
    font_cfg.SizePixels = persistent_float["fFontScale"];
    io.Fonts->AddFontDefault(&font_cfg);

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));

    bImguiSetup = true;
}

DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);

    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);

    screenCenter = Vector2(Width / 2, Height / 2);

    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();
    if (IsExpired()) {
        DrawExpired(io);
    } else if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        DrawFloatingButton(io);
        DrawMenu(io);
// "Powered By @Zoroo_God" — Auto-centrat jos
{
    // Punem poziția la X = mijloc, Y = aproape de marginea de jos (Height - 30px)
    // Pivotul (0.5f, 1.0f) centrează orizontal și lipește marginea de jos a textului de coordonata Y
    SetNextWindowPos(ImVec2(Width * 0.5f, Height - 30.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    
    // Fereastră fără fundal, fără margini, care se redimensionează singură
    Begin(O("##PoweredBy"), nullptr, 
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | 
          ImGuiWindowFlags_NoInputs);
    
    {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImDrawList* fgDl = ImGui::GetWindowDrawList();
        const char lbl[] = "@LYN4XP";
        ImVec2 ts2 = ImGui::CalcTextSize(lbl);
        fgDl->AddRectFilled(
            ImVec2(cp.x - 8, cp.y - 3),
            ImVec2(cp.x + ts2.x + 8, cp.y + ts2.y + 3),
            IM_COL32(3, 18, 10, 180), 8.0f);
        fgDl->AddRect(
            ImVec2(cp.x - 8, cp.y - 3),
            ImVec2(cp.x + ts2.x + 8, cp.y + ts2.y + 3),
            IM_COL32(12, 235, 107, 125), 8.0f, 0, 1.2f);
        fgDl->AddText(ImVec2(cp.x + 1, cp.y + 1), IM_COL32(0, 40, 15, 120), lbl);
        ImGui::TextColored(ImVec4(0.06f, 0.95f, 0.42f, 0.88f), "%s", lbl);
    }
    
    End();
}

        if (persistent_bool[O("bAutoAim")] && g_espIsInGame) {
            DrawAimInfoOverlay(io);
            DrawWinCenterBanner(io);
        }
    } else {
        DrawLogin(io);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui_ClearHoverEffect();

    return _Draw(dpy, surface);
}

void __IMGUI__() {
    create_directory_recursive(CONC(O("/data/user_de/0/"), PACKAGE_NAME.c_str(), O("/no_backup")));
}
