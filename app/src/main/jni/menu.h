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

// Obfuscated expiry date: 13.04.2026 00:00:00 UTC
static const int64_t EXPIRY_TS = O(1780099200LL);

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

static bool SidebarButton(const char* label, GLuint iconTex, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float iconSize   = 60.0f;
    float vPad       = 10.0f;
    float btnH       = vPad + iconSize + 4.0f + g.FontSize + vPad;

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, btnH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // Icon center position
    float iconBgPad  = 6.0f;
    float iconBgSize = iconSize + iconBgPad * 2.0f;
    ImVec2 iconCenter = ImVec2(
        bb.Min.x + width * 0.5f,
        bb.Min.y + vPad + iconSize * 0.5f
    );

    // Selected: blue rounded rect behind icon only
    if (selected) {
        dl->AddRectFilled(
            ImVec2(iconCenter.x - iconBgSize * 0.5f, iconCenter.y - iconBgSize * 0.5f),
            ImVec2(iconCenter.x + iconBgSize * 0.5f, iconCenter.y + iconBgSize * 0.5f),
            IM_COL32(30, 100, 220, 255), 12.0f
        );
    }

    // Draw icon texture centered, with color filter when not selected
    if (iconTex) {
        ImVec2 iconMin = ImVec2(iconCenter.x - iconSize * 0.5f, iconCenter.y - iconSize * 0.5f);
        ImVec2 iconMax = ImVec2(iconCenter.x + iconSize * 0.5f, iconCenter.y + iconSize * 0.5f);
        ImU32 tint = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 255);
        dl->AddImage((void*)(intptr_t)iconTex, iconMin, iconMax, ImVec2(0,0), ImVec2(1,1));
    }

    // Draw label centered below icon
    ImVec2 labelSize = CalcTextSize(label);
    ImVec2 textPos   = ImVec2(
        bb.Min.x + (width - labelSize.x) * 0.5f,
        bb.Min.y + vPad + iconSize + 4.0f
    );
    ImU32 textCol = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(140, 140, 150, 255);
    dl->AddText(textPos, textCol, label);

    return pressed;
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float scale = 1.5f; // 1.5f is with 50% bigger than writen values
    float height = 32.0f * scale;
    float width = 56.0f * scale;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, ImMax(height, textSize.y) + style.FramePadding.y * 2 + 10.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> switchAnim;
    float& animT = switchAnim[id];
    float targetT = *v ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 14.0f;

    ImDrawList* dl = window->DrawList;
    
    if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(45, 45, 55, 100), 10.0f);
    }
    
    ImVec2 togglePos = ImVec2(bb.Max.x - width - 15.0f, bb.Min.y + (size.y - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + width, togglePos.y + height);
    
    ImVec4 offColor = ImVec4(0.27f, 0.27f, 0.31f, 1.0f);
    ImVec4 onColor = ImVec4(0.12f, 0.45f, 0.95f, 1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, animT);
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);
    
    float knobX = togglePos.x + radius + (width - height) * animT;
    float knobY = togglePos.y + radius;
    float knobR = radius - 4.0f;
    
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 2.0f, IM_COL32(0, 0, 0, 40));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));

    dl->AddText(ImVec2(bb.Min.x + 15.0f, bb.Min.y + (size.y - textSize.y) * 0.5f), IM_COL32(230, 230, 240, 255), label);

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
static bool g_autoPlayCalculating = false;

// Game state flags — set by DrawESP (inside sigsetjmp), read by DrawMenu (outside)
static bool g_espStateReady = false;
static bool g_espIsInGame   = false;

#include "mod/ButtonClicker.h"
#include "game/inc/AutoPlay.h"
#include "game/inc/AimLockTarget.h"
#include "game/inc/AimBreak.h"
#include "game/inc/Aim9Ball.h"
#include "game/inc/Aim9BallBreak.h"

// Active aim mode
enum class AimMode : int {
    NONE              = 0,
    EIGHTBALL_PREDICT = 1,
    EIGHTBALL_BREAK   = 2,
    NINEBALL_PREDICT  = 3,
    NINEBALL_BREAK    = 4,
};
static AimMode g_aimMode = AimMode::EIGHTBALL_PREDICT;

// Aim overlay info (filled after each aim calculation)
struct AimOverlayInfo {
    const char* modeName  = "---";
    int  targetBall       = -1;
    int  targetPocket     = -1;
    int  ballsFound       = 0;
    bool isCombo          = false;
    bool isWin9           = false;
    bool isCalculating    = false;
};
static AimOverlayInfo g_aimOverlay;


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

        if (persistent_bool[O("bESP_DrawPockets")]) {
            for (int i = 0; i < 6; i++) {
                auto screenPos = WorldToScreen(pockets[i]);
                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 40, WHITE, 0, 3.f);
            }
        }

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        if (persistent_bool[O("bAutoAim")]) {
            AutoAim::Update();
        }

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4) gPrediction->determineShotResult(false);

        // Enemy Line: gambar prediksi tembakan lawan saat giliran lawan (state 7)
        if (stateId == 7 && persistent_bool[O("bESP_EnemyLine")]) {
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

        if (persistent_bool[O("bESP_DrawPredictionLine")]) {
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

        if (persistent_bool[O("bESP_DrawPredictionLine")]) {
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
    static GLuint draw_icon_tex = LoadTextureFromMemory(draw_icon_png, draw_icon_png_len);
    static GLuint play_icon_tex = LoadTextureFromMemory(play_icon_png, play_icon_png_len);
    static GLuint q_icon_tex    = LoadTextureFromMemory(q_icon_png,    q_icon_png_len);
    static GLuint user_icon_tex = LoadTextureFromMemory(user_icon_png, user_icon_png_len);

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
    if (SidebarButton(O("Draw"),   draw_icon_tex, g_menu.currentTab == 0, btnW)) g_menu.currentTab = 0;
    SameLine(0, 0);
    if (SidebarButton(O("8 Ball"), play_icon_tex, g_menu.currentTab == 1, btnW)) g_menu.currentTab = 1;
    SameLine(0, 0);
    if (SidebarButton(O("9 Ball"), q_icon_tex,    g_menu.currentTab == 2, btnW)) g_menu.currentTab = 2;
    SameLine(0, 0);
    if (SidebarButton(O("Info"),   user_icon_tex, g_menu.currentTab == 3, btnW)) g_menu.currentTab = 3;
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

// ── Aim Info Overlay — top-left animated panel ────────────────────────────────
// Shows: mode name, target ball, status, smooth pulsing animation
// Called every frame when Auto Aim is active and in-game
static void DrawAimInfoOverlay(ImGuiIO& io) {
    float t = (float)GetTime();

    // Mode display name and accent color
    const char* modeName  = O("---");
    ImVec4      modeColor = ImVec4(0.50f, 0.70f, 1.0f, 1.0f);
    switch (g_aimMode) {
        case AimMode::EIGHTBALL_PREDICT: modeName = O("8BP Predict");  modeColor = ImVec4(0.30f, 0.65f, 1.0f, 1.0f); break;
        case AimMode::EIGHTBALL_BREAK:   modeName = O("8BP Break");    modeColor = ImVec4(1.0f,  0.55f, 0.10f, 1.0f); break;
        case AimMode::NINEBALL_PREDICT:  modeName = O("9BP Predict");  modeColor = ImVec4(0.20f, 0.90f, 0.60f, 1.0f); break;
        case AimMode::NINEBALL_BREAK:    modeName = O("9BP Break Win");modeColor = ImVec4(1.0f,  0.85f, 0.10f, 1.0f); break;
        default: break;
    }

    // Status text + color
    const char* statusText;
    ImVec4 statusColor;
    if (g_autoPlayCalculating) {
        statusText  = O("ANALYZING...");
        statusColor = ImVec4(1.0f, 0.80f, 0.0f, 1.0f);
    } else if (AutoAim::bAimed) {
        statusText  = O("AIMED");
        statusColor = ImVec4(0.20f, 1.0f, 0.45f, 1.0f);
    } else {
        statusText  = O("READY");
        statusColor = ImVec4(0.55f, 0.70f, 1.0f, 1.0f);
    }

    // Target ball display
    char ballBuf[32] = "---";
    int tgtBall = -1;
    switch (g_aimMode) {
        case AimMode::EIGHTBALL_PREDICT: tgtBall = AimLockTarget::lastTargetBall;   break;
        case AimMode::EIGHTBALL_BREAK:   tgtBall = AimBreak::lastTargetBall;        break;
        case AimMode::NINEBALL_PREDICT:  tgtBall = Aim9Ball::lastTargetBall;        break;
        case AimMode::NINEBALL_BREAK:    tgtBall = Aim9BallBreak::lastTargetBall;   break;
        default: break;
    }
    if (tgtBall >= 0) snprintf(ballBuf, sizeof(ballBuf), O("Ball %d"), tgtBall);

    // Extra info per mode
    char extraBuf[64] = "";
    switch (g_aimMode) {
        case AimMode::EIGHTBALL_BREAK: {
            if (AimBreak::lastBestCount > 0)
                snprintf(extraBuf, sizeof(extraBuf), O("Pocket: %d balls"), AimBreak::lastBestCount);
            break;
        }
        case AimMode::NINEBALL_PREDICT: {
            if (Aim9Ball::lastWasCombo) snprintf(extraBuf, sizeof(extraBuf), O("COMBO / WIN 9!"));
            break;
        }
        case AimMode::NINEBALL_BREAK: {
            if (Aim9BallBreak::lastWasWin9)        snprintf(extraBuf, sizeof(extraBuf), O("WIN 9 FOUND!"));
            else if (Aim9BallBreak::lastBestCount > 0) snprintf(extraBuf, sizeof(extraBuf), O("Best: %d balls"), Aim9BallBreak::lastBestCount);
            break;
        }
        default: break;
    }

    // Panel size
    float panelW = 270.0f;
    float margin  = 22.0f;

    SetNextWindowPos(ImVec2(margin, margin), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(panelW, 0), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(8, 14, 32, 220));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(30, 80, 200, 200));
    PushStyleVar(ImGuiStyleVar_WindowRounding,  16.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(14.0f, 12.0f));

    if (Begin(O("##AimInfo"), nullptr,
              ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
              ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl  = GetWindowDrawList();
        ImVec2      wpos = GetWindowPos();
        ImVec2      wsz  = GetWindowSize();

        // ── Animated accent line at top ───────────────────────────────────
        float lineAnim = (sinf(t * 2.5f) + 1.0f) * 0.5f; // 0..1 pulse
        ImU32 lineColA = ImColor(modeColor.x, modeColor.y, modeColor.z, 0.5f + 0.5f * lineAnim);
        ImU32 lineColB = ImColor(modeColor.x, modeColor.y, modeColor.z, 0.0f);
        dl->AddRectFilledMultiColor(
            wpos, ImVec2(wpos.x + wsz.x, wpos.y + 3.0f),
            lineColA, lineColB, lineColB, lineColA);

        // ── Spinning dots indicator ───────────────────────────────────────
        {
            ImVec2 dotCenter = ImVec2(
                wpos.x + 14.0f + 10.0f,
                GetCursorScreenPos().y + GImGui->FontSize * 0.5f
            );
            float dotR  = 5.5f;
            float orbit = 12.0f;
            int   nDots = 8;
            for (int i = 0; i < nDots; i++) {
                float angle  = t * 4.0f + (float)i * (2.0f * 3.14159f / nDots);
                float alpha  = (g_autoPlayCalculating || AutoAim::bAimed)
                               ? (0.25f + 0.75f * ((float)i / nDots))
                               : 0.18f;
                ImVec2 dp = ImVec2(dotCenter.x + cosf(angle) * orbit,
                                   dotCenter.y + sinf(angle) * orbit);
                dl->AddCircleFilled(dp, dotR * alpha * 1.2f,
                    ImColor(modeColor.x, modeColor.y, modeColor.z, alpha));
            }
            // Keep cursor space for dots
            SetCursorPosX(GetCursorPosX() + 30.0f);
        }

        // ── Mode name ─────────────────────────────────────────────────────
        SameLine(0, 32.0f);
        SetWindowFontScale(1.10f);
        TextColored(modeColor, "%s", modeName);
        SetWindowFontScale(1.0f);

        Dummy(ImVec2(0, 4));

        // ── Status badge ──────────────────────────────────────────────────
        {
            // Pulsing alpha for ANALYZING
            float badgeAlpha = g_autoPlayCalculating
                ? (0.65f + 0.35f * sinf(t * 8.0f)) : 1.0f;
            ImVec4 bc = statusColor;
            bc.w = badgeAlpha;

            ImVec2 badgePos = GetCursorScreenPos();
            ImVec2 badgeSz  = CalcTextSize(statusText);
            float  pad      = 6.0f;
            dl->AddRectFilled(
                ImVec2(badgePos.x - pad, badgePos.y - 2.0f),
                ImVec2(badgePos.x + badgeSz.x + pad, badgePos.y + badgeSz.y + 2.0f),
                ImColor(bc.x * 0.3f, bc.y * 0.3f, bc.z * 0.3f, 0.5f * badgeAlpha), 6.0f);

            SetWindowFontScale(1.05f);
            TextColored(bc, "%s", statusText);
            SetWindowFontScale(1.0f);
        }

        // ── Target ball ───────────────────────────────────────────────────
        Dummy(ImVec2(0, 4));
        TextColored(ImVec4(0.45f, 0.48f, 0.58f, 1.0f), O("Target: "));
        SameLine();
        TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), "%s", ballBuf);

        // ── Extra info (combo / pocket count) ────────────────────────────
        if (extraBuf[0] != '\0') {
            Dummy(ImVec2(0, 2));
            // Pulsing glow for win/combo messages
            float glow = 0.75f + 0.25f * sinf(t * 5.0f);
            TextColored(ImVec4(
                modeColor.x * glow, modeColor.y * glow, modeColor.z * glow, 1.0f),
                "%s", extraBuf);
        }

        Dummy(ImVec2(0, 4));
    }
    End();
    PopStyleVar(3);
    PopStyleColor(2);
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
    
    const char* tabTitles[] = {
        O("Draw Settings"),
        O("8 Ball"),
        O("9 Ball"),
        O("Info")
    };

    // --- CENTRARE TITLU TAB ---
    const char* currentTitle = tabTitles[g_menu.currentTab];
    float titlePadT = 18.0f;
    float titlePadB = 12.0f;

    // 1. Setăm scara fontului înainte de calcul
    SetWindowFontScale(1.15f);
    ImVec2 ts = CalcTextSize(currentTitle);
    
    // 2. Calculăm X pentru centrare: (Lățime fereastră - Lățime text) / 2
    float centeredX = (contentW - ts.x) * 0.5f;
    SetCursorPos(ImVec2(centeredX, startY + titlePadT));
    
    // 3. Afișăm textul
    TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", currentTitle);
    SetWindowFontScale(1.0f); // Resetăm imediat

    // Linie separatoare centrată și ea (lăsăm 20px margini)
    float lineY = startY + titlePadT + ts.y + titlePadB;
    dl->AddLine(
        ImVec2(wp.x + 20.0f, wp.y + lineY),
        ImVec2(wp.x + contentW - 20.0f, wp.y + lineY),
        IM_COL32(40, 100, 220, 180), 1.0f
    );

    float headerH = (lineY - startY) + 10.0f;
    SetCursorPos(ImVec2(10.0f, startY + headerH));
    
    // Începutul zonei de child (conținutul propriu-zis)
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false);
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Draw Lines"), &persistent_bool[O("bESP_DrawPredictionLine")]);
           // need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPockets")]);
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

            if (ToggleSwitch(O("Enable Auto Aim"), &persistent_bool[O("bAutoAim")])) {
                AutoAim::bActive = persistent_bool[O("bAutoAim")];
                need_save = true;
            }

            Dummy(ImVec2(0, 12));
            PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
            TextColored(ImVec4(0.42f, 0.42f, 0.50f, 1.0f),
                O("Tap in-game button to calculate aim once. Shoot manually."));
            PopTextWrapPos();
            Dummy(ImVec2(0, 18));

            auto AimBtn8 = [&](const char* label, AimMode mode, const char* desc) {
                bool sel = (g_aimMode == mode);
                PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
                if (sel) {
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.38f, 0.90f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.48f, 1.00f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f, 0.28f, 0.72f, 1.0f));
                } else {
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.13f, 0.17f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.26f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.09f, 0.13f, 1.0f));
                }
                if (Button(label, ImVec2(GetContentRegionAvail().x, 58.0f)))
                    g_aimMode = mode;
                PopStyleColor(3);
                PopStyleVar();
                Dummy(ImVec2(0, 3));
                PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
                TextColored(sel
                    ? ImVec4(0.55f, 0.80f, 1.0f, 1.0f)
                    : ImVec4(0.38f, 0.38f, 0.46f, 1.0f), "%s", desc);
                PopTextWrapPos();
                Dummy(ImVec2(0, 14));
            };

            AimBtn8(O("  Aim Break"),
                AimMode::EIGHTBALL_BREAK,
                O("Find break angle that pockets the most balls (no 8-ball foul)."));

            AimBtn8(O("  Aim Predict"),
                AimMode::EIGHTBALL_PREDICT,
                O("Aim at your solid/stripe. Cue must hit your ball first. Avoids opponent balls."));

            break;
        }

        case 2: {
            // ── 9 BALL POOL ───────────────────────────────────────────────
            Dummy(ImVec2(0, 10));

            if (ToggleSwitch(O("Enable Auto Aim"), &persistent_bool[O("bAutoAim")])) {
                AutoAim::bActive = persistent_bool[O("bAutoAim")];
                need_save = true;
            }

            Dummy(ImVec2(0, 12));
            PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
            TextColored(ImVec4(0.42f, 0.42f, 0.50f, 1.0f),
                O("Tap in-game button to calculate aim once. Shoot manually."));
            PopTextWrapPos();
            Dummy(ImVec2(0, 18));

            auto AimBtn9 = [&](const char* label, AimMode mode, const char* desc) {
                bool sel = (g_aimMode == mode);
                PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
                if (sel) {
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.65f, 0.45f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.78f, 0.55f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f, 0.50f, 0.35f, 1.0f));
                } else {
                    PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.13f, 0.17f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.26f, 1.0f));
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.09f, 0.13f, 1.0f));
                }
                if (Button(label, ImVec2(GetContentRegionAvail().x, 58.0f)))
                    g_aimMode = mode;
                PopStyleColor(3);
                PopStyleVar();
                Dummy(ImVec2(0, 3));
                PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
                TextColored(sel
                    ? ImVec4(0.40f, 1.0f, 0.72f, 1.0f)
                    : ImVec4(0.38f, 0.38f, 0.46f, 1.0f), "%s", desc);
                PopTextWrapPos();
                Dummy(ImVec2(0, 14));
            };

            AimBtn9(O("  Aim Break One Shot Win"),
                AimMode::NINEBALL_BREAK,
                O("Break shot that tries to pocket ball 9 immediately for instant win."));

            AimBtn9(O("  Aim Predict + Combo"),
                AimMode::NINEBALL_PREDICT,
                O("Hit lowest ball first. Find 2+ ball combo. Priority: pocket ball 9."));

            break;
        }
        
        case 3: {
            // ── INFO ──────────────────────────────────────────────────────────
            auto DrawSecHdr = [&](const char* title) {
                Dummy(ImVec2(0, 12));
                float avail = GetContentRegionAvail().x;
                ImVec2 p    = GetCursorScreenPos();
                float  fs   = GImGui->FontSize;
                ImVec2 ts   = CalcTextSize(title);
                float  lineY = p.y + fs * 0.5f;
                float  gap   = 8.0f;
                float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
                ImDrawList* dl2 = GetWindowDrawList();
                dl2->AddLine(ImVec2(p.x, lineY), ImVec2(p.x + lineW, lineY), IM_COL32(40,100,220,160), 1.0f);
                dl2->AddLine(ImVec2(p.x + lineW + gap + ts.x + gap, lineY), ImVec2(p.x + avail, lineY), IM_COL32(40,100,220,160), 1.0f);
                SetCursorPosX(GetCursorPosX() + lineW + gap);
                TextColored(ImVec4(0.50f, 0.55f, 0.70f, 1.0f), "%s", title);
                Dummy(ImVec2(0, 6));
            };
            auto DrawRow = [&](const char* key, const char* val) {
                TextColored(ImVec4(0.45f, 0.48f, 0.58f, 1.0f), "%s", key);
                SameLine();
                TextColored(ImVec4(0.88f, 0.90f, 0.96f, 1.0f), "%s", val);
                Dummy(ImVec2(0, 3));
            };

            // ── Mod Info ─────────────────────────────────────────────────────
            DrawSecHdr(O("Mod Info"));
            DrawRow(O("Engine:  "), O("Flux Pro Engine v2.0"));
            DrawRow(O("Game:    "), O("8 Ball Pool"));
            DrawRow(O("Arch:    "), O("arm64-v8a"));
            DrawRow(O("Build:   "), O(__DATE__ " " __TIME__));

            {
                int64_t now_ts = (int64_t)time(nullptr);
                int64_t diff   = EXPIRY_TS - now_ts;
                char expBuf[64];
                if (diff > 0) {
                    int days  = (int)(diff / 86400);
                    int hours = (int)((diff % 86400) / 3600);
                    int mins  = (int)((diff % 3600)  / 60);
                    snprintf(expBuf, sizeof(expBuf), "%dd %dh %dm", days, hours, mins);
                } else {
                    snprintf(expBuf, sizeof(expBuf), "%s", O("EXPIRED"));
                }
                DrawRow(O("Expires: "), expBuf);
            }

            // ── Telegram / Owner ──────────────────────────────────────────────
            DrawSecHdr(O("Contact & Owner"));
            Dummy(ImVec2(0, 4));

            {
                float cardW = GetContentRegionAvail().x;
                ImVec2 cPos = GetCursorScreenPos();
                ImDrawList* dlx = GetWindowDrawList();

                // Gradient banner
                dlx->AddRectFilled(cPos, ImVec2(cPos.x + cardW, cPos.y + 72),
                    IM_COL32(14, 40, 90, 220), 12.0f);
                dlx->AddRectFilled(ImVec2(cPos.x + cardW * 0.5f, cPos.y),
                    ImVec2(cPos.x + cardW, cPos.y + 72),
                    IM_COL32(0, 90, 180, 100), 12.0f);

                // Owner text
                SetWindowFontScale(1.25f);
                ImVec2 t1 = CalcTextSize(O("@LYN4XP"));
                dlx->AddText(GImGui->Font, GImGui->FontSize * 1.25f,
                    ImVec2(cPos.x + (cardW - t1.x) * 0.5f, cPos.y + 10),
                    IM_COL32(80, 200, 255, 255), O("@LYN4XP"));
                SetWindowFontScale(1.0f);

                ImVec2 t2 = CalcTextSize(O("t.me/Lyn4xp"));
                dlx->AddText(GImGui->Font, GImGui->FontSize,
                    ImVec2(cPos.x + (cardW - t2.x) * 0.5f, cPos.y + 44),
                    IM_COL32(160, 210, 255, 200), O("t.me/Lyn4xp"));

                Dummy(ImVec2(cardW, 72));
            }

            Dummy(ImVec2(0, 10));
            PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
            TextColored(ImVec4(0.85f, 0.20f, 0.20f, 1.0f),
                O("FREE BETA. If you paid for this, you were SCAMMED."));
            PopTextWrapPos();

            // ── Device Info ───────────────────────────────────────────────────
            DrawSecHdr(O("Device"));
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
                DrawRow(O("Maker:   "), s_mfr);
                DrawRow(O("Model:   "), s_mdl);
                DrawRow(O("ABI:     "), s_abi);
                DrawRow(O("Android: "), s_and);
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
        // Auto-open menu on first frame when bypassing login
        if (DEBUG_BYPASS_LOGIN) {
            static bool s_autoOpened = false;
            if (!s_autoOpened) { g_menu.isOpen = true; s_autoOpened = true; }
        }

        buttonClicker.Update();

        // Reset game state flags before each frame's DrawESP
        g_espStateReady = false;

        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        // ── ImGui overlay windows — OUTSIDE sigsetjmp guard ─────────────────
        // DrawToggleButton opens an ImGui::Begin/End window.
        // Calling it outside the sigsetjmp ensures the window stack stays
        // clean even if DrawESP crashes mid-frame.
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
            PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
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
            
            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

// Toggle button shown in-game — tap once to enable/disable AutoAim (aim stays on until tapped again)
static void DrawToggleButton() {
    ImGuiIO& io = GetIO();

    static GLuint play_on_tex  = LoadTextureFromMemory(play_on_png,  play_on_png_len);
    static GLuint play_off_tex = LoadTextureFromMemory(play_off_png, play_off_png_len);

    float button_size   = 130.f;
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

        // play_on = aimed/calculating, play_off = idle (ready to aim)
        GLuint tex = AutoAim::bAimed ? play_on_tex : play_off_tex;

        float r = size.x * 0.5f;
        ImDrawList* dl = GetWindowDrawList();
        dl->AddImage((void*)(intptr_t)tex,
            ImVec2(center.x - r, center.y - r),
            ImVec2(center.x + r, center.y + r));

        // Vertical-only drag
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - windowHeight);
            SetWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);
        }

        if (clicked) {
            g_autoPlayCalculating = true;
            AutoAim::bAimed       = false;
            switch (g_aimMode) {
                case AimMode::EIGHTBALL_PREDICT:
                    AimLockTarget::Aim();
                    AutoAim::bAimed       = true;
                    g_autoPlayCalculating = false;
                    break;
                case AimMode::EIGHTBALL_BREAK:
                    AimBreak::Aim();
                    AutoAim::bAimed       = true;
                    g_autoPlayCalculating = false;
                    break;
                case AimMode::NINEBALL_PREDICT:
                    Aim9Ball::Aim();
                    AutoAim::bAimed       = true;
                    g_autoPlayCalculating = false;
                    break;
                case AimMode::NINEBALL_BREAK:
                    Aim9BallBreak::Aim();
                    AutoAim::bAimed       = true;
                    g_autoPlayCalculating = false;
                    break;
                default:
                    AutoAim::TriggerAim();
                    break;
            }
        }
    }
    End();
    PopStyleVar();
    PopStyleColor(2);
}

static void DrawFloatingButton(ImGuiIO& io) {
    static GLuint logo_tex   = LoadTextureFromMemory(logo_png, logo_png_len);
    static bool   isDragging = false;

    float buttonRadius = 65.0f;
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

        ImDrawList* dl     = GetWindowDrawList();
        ImVec2      center = ImVec2(fixedX + buttonRadius + 5, posY + buttonRadius + 5);

        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));

        // Draw logo — no animations, fixed size
        dl->AddImage((void*)(intptr_t)logo_tex,
                     ImVec2(center.x - buttonRadius, center.y - buttonRadius),
                     ImVec2(center.x + buttonRadius, center.y + buttonRadius));

        // Vertical-only drag moves both buttons together via g_sideBtnsY
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
    
    TextColored(ImColor(51, 140, 255, 255), O("@LYN4XP"));
    
    End();
}

        if (persistent_bool[O("bAutoAim")] && g_espIsInGame)
            DrawAimInfoOverlay(io);
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
