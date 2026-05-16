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
#include "game/inc/AutoPlay.h"
#include "mod/dumplib.h"

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

    // Selected: modern accent highlight behind icon
    if (selected) {
        // Soft glow fill
        dl->AddRectFilled(
            ImVec2(iconCenter.x - iconBgSize * 0.5f, iconCenter.y - iconBgSize * 0.5f),
            ImVec2(iconCenter.x + iconBgSize * 0.5f, iconCenter.y + iconBgSize * 0.5f),
            IM_COL32(200, 30, 30, 200), 14.0f
        );
        // Bright inner ring
        dl->AddRect(
            ImVec2(iconCenter.x - iconBgSize * 0.5f + 2, iconCenter.y - iconBgSize * 0.5f + 2),
            ImVec2(iconCenter.x + iconBgSize * 0.5f - 2, iconCenter.y + iconBgSize * 0.5f - 2),
            IM_COL32(255, 80, 80, 120), 12.0f, 0, 1.5f
        );
    } else if (hovered) {
        dl->AddRectFilled(
            ImVec2(iconCenter.x - iconBgSize * 0.5f, iconCenter.y - iconBgSize * 0.5f),
            ImVec2(iconCenter.x + iconBgSize * 0.5f, iconCenter.y + iconBgSize * 0.5f),
            IM_COL32(60, 60, 75, 120), 14.0f
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
    ImVec4 onColor = ImVec4(1.0f, 0.f, 0.f, 1.0f);
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

// File-scope so DrawToggleButton cancel can also reset countdown
static bool g_aqCounting = false;
static std::chrono::steady_clock::time_point g_aqLastCall;
static std::chrono::steady_clock::time_point g_aqCountdownStart;


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
            O("Beta Version Expired. Update on our Telegram Your Id"));
        PopTextWrapPos();

        Dummy(ImVec2(0, 10));
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}

INLINE void DrawAutoQueue() {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqLastCall).count() > 500)
            g_aqCounting = false;
        g_aqLastCall = now;

        if (!g_aqCounting) {
            g_aqCounting = true;
            g_aqCountdownStart = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqCountdownStart).count();
        int remaining_ms = 8000 - (int)elapsed;

      // if (remaining_ms <= 0) {
           // if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            //StartLastMatch();
           // g_aqCounting = false;
          //  return;
       // }

        std::string count_str = std::to_string((remaining_ms / 1000) + 1);

        // Minimal auto-sized window, transparent bg — we draw our own rounded rect
        SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1.f));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 20.0f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 24.0f);

        if (Begin(O("##AutoQueueCD"), nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_AlwaysAutoResize)) {
            ImDrawList* dl  = GetWindowDrawList();
            ImVec2      wp  = GetWindowPos();
            ImVec2      ws  = GetWindowSize();
            dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(20, 20, 28, 0), 24.0f);

            SetWindowFontScale(3.5f);
            TextColored(ImVec4(1.f, 0.f, 0.f, 1.0f), "%s", count_str.c_str());
            SetWindowFontScale(1.0f);
        }
        End();
        PopStyleVar(2);
        PopStyleColor();
    }
}

#include "mod/ButtonClicker.h"

static void DrawToggleButton(bool cancelMode); // forward declaration — defined after DrawFloatingButton

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
        if (!mainStateManager.isInGame()) {
        if (persistent_bool[O("bAutoQueue")]) {
            if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
            DrawToggleButton(true);  // acts as cancel button for autoqueue
        } return;
        }

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto& pockets = tableProperties.mPockets();

        if (persistent_bool[O("bESP_DrawPockets")]) {
            for (int i = 0; i < 6; i++) {
                auto screenPos = WorldToScreen(pockets[i]);
                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 40, WHITE, 0, 3.f);
            }
        }

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        if (persistent_bool[O("bAutoPlay")]) {
            DrawToggleButton(false);
            AutoPlay::Update();
        }

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4) gPrediction->determineShotResult(false);
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

        // ── Enemy Line — orange trajectories for OPPONENT balls only ────────
        if (persistent_bool[O("bESP_EnemyLine")]) {
            bool is9Ball = sharedGameManager.is9BallGame();
            // 8-ball: show only the opposing classification; 9-ball: show all numbered balls
            Ball::Classification enemyClass = Ball::Classification::ANY;
            if (!is9Ball) {
                if (myclass == Ball::Classification::SOLID)
                    enemyClass = Ball::Classification::STRIPE;
                else if (myclass == Ball::Classification::STRIPE)
                    enemyClass = Ball::Classification::SOLID;
            }
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (!ball.originalOnTable) continue;
                // In 8-ball mode filter to opponent class only
                if (!is9Ball && ball.classification != enemyClass) continue;
                if (ball.initialPosition == ball.predictedPosition) continue;
                auto ip = WorldToScreen(ball.initialPosition);
                auto pp = WorldToScreen(ball.predictedPosition);
                draw->AddLine(ImVec2(ip.x, ip.y), ImVec2(pp.x, pp.y),
                              IM_COL32(255, 130, 0, 175), 3.0f);
                draw->AddCircle(ImVec2(pp.x, pp.y), 22.f,
                                IM_COL32(255, 145, 0, 235), 0, 3.5f);
                draw->AddCircleFilled(ImVec2(pp.x, pp.y), 8.f,
                                     IM_COL32(255, 100, 0, 245));
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
    float btnW      = tabsW / 5.0f;
    float marginB   = 12.0f;

    // Split channels: 0 = background (drawn last, appears behind), 1 = buttons (drawn first)
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    // Draw tab buttons — let ImGui lay them out naturally
    BeginGroup();
    SetCursorPos(ImVec2(0.0f, 0.0f));
    if (SidebarButton(O("Draw"),  draw_icon_tex, g_menu.currentTab == 0, btnW)) g_menu.currentTab = 0;
    SameLine(0, 0);
    if (SidebarButton(O("Play"),  play_icon_tex, g_menu.currentTab == 1, btnW)) g_menu.currentTab = 1;
    SameLine(0, 0);
    if (SidebarButton(O("Queue"), q_icon_tex,    g_menu.currentTab == 2, btnW)) g_menu.currentTab = 2;
    SameLine(0, 0);
    if (SidebarButton(O("User"),  user_icon_tex, g_menu.currentTab == 3, btnW)) g_menu.currentTab = 3;
    SameLine(0, 0);
    if (SidebarButton(O("Dump"),  user_icon_tex, g_menu.currentTab == 4, btnW)) g_menu.currentTab = 4;
    EndGroup();

    // Measure actual rendered height — this is the true wrap_content
    float sidebarH = GetItemRectMax().y - wp.y;

    // Now draw background on channel 0 (behind the buttons)
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(wp, ImVec2(wp.x + sidebarW, wp.y + sidebarH), IM_COL32(21, 21, 21, 255), 30.0f);
    dl->ChannelsMerge();

    // Vertical separator between Queue and close strip
    float sepX       = wp.x + sidebarW - closeBtnW;
    float sepCenterY = wp.y + sidebarH * 0.5f;
    float sepHalfH   = sidebarH * 0.28f;
    dl->AddLine(
        ImVec2(sepX, sepCenterY - sepHalfH),
        ImVec2(sepX, sepCenterY + sepHalfH),
        IM_COL32(60, 60, 75, 200), 1.5f
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

// Floating button position — both X and Y, draggable freely
static float g_btnX           = -1.0f;
static float g_btnY           = -1.0f;
// Keep g_sideBtnsY alias so nothing else breaks
static float& g_sideBtnsY     = g_btnY;
// Kept for linker compatibility
static float g_toggleRotAngle = 0.0f;
// CALCULATING overlay (unused now — AutoPlay simplified)
static bool  g_autoPlayCalculating = false;
// Main menu saved position (-1 = not yet initialised)
static float g_menuPosX = -1.0f;
static float g_menuPosY = -1.0f;

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

// ── CALCULATING overlay (shown during AutoPlay SLOW scan) ─────────────────────
static void DrawCalculating(ImGuiIO& io) {
    // Setăm poziția pe centrul ecranului (Width*0.5, Height*0.5)
    // Pivotul (0.5f, 0.5f) înseamnă că mijlocul ferestrei va fi fix pe coordonatele date
    SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    // Auto-resize face ca fereastra să aibă dimensiunea textului automat
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleColor(ImGuiCol_Border, IM_COL32(220, 30, 30, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    if (Begin(O("##CalcOverlay"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs)) {
        
        SetWindowFontScale(1.4f);
        TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), O("CALCULATING..."));
        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor(2);
}

// ── AutoPlay dependencies ─────────────────────────────────────────────────────
#include "game/inc/AutoAim.h"
#include "mod/PowerSlider.h"

// ── NineBall One Shoot ────────────────────────────────────────────────────────
// Rules enforced:
//   • Must hit LOWEST numbered ball first every shot (legal play)
//   • If 9-ball can be combo-pocketed → instant win shot preferred
//   • No scratch (cue ball stays on table)
//
// PERFORMANCE: AIM is calculated ONCE per turn (bCalculated flag).
//   angleStep = 0.25 rad → max ~25 physics-sim iterations total (was 125/frame).
namespace NineBall {

    inline bool  bCalculated = false; // true = aim angle already set for this turn
    inline int   g_prevStateId = -1;  // detect turn transitions
    inline float g_spinAngle   = 0.f; // thinking spinner animation angle

    static int findLowestBall() {
        for (int i = 1; i < gPrediction->guiData.ballsCount && i <= 9; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.originalOnTable && b.classification == Ball::Classification::NINE_BALL_RULE)
                return i;
        }
        return -1;
    }

    // step = 0.25 rad → 25 iterations max for full circle (vs 125 at step=0.05)
    static bool AIM_Combo(int lowestIdx, double step = 0.25) {
        if (gPrediction->guiData.ballsCount <= 9) return false;
        if (!gPrediction->guiData.balls[9].originalOnTable) return false;

        double base = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        for (double a = NumberUtils::normalizeDoublePrecision(normalizeAngle(base + step));
             a != base;
             a = NumberUtils::normalizeDoublePrecision(normalizeAngle(a + step))) {

            gPrediction->determineShotResult(true, a);
            auto& cue  = gPrediction->guiData.balls[0];
            auto& b9   = gPrediction->guiData.balls[9];
            if (!cue.onTable)  continue;
            if (!b9.originalOnTable || b9.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (gPrediction->guiData.collision.firstHitBall->index != lowestIdx) continue;
            AutoAim::setAimAngle(a);
            return true;
        }
        return false;
    }

    static void AIM_Standard(int lowestIdx, double step = 0.25) {
        double base = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());

        for (double a = NumberUtils::normalizeDoublePrecision(normalizeAngle(base + step));
             a != base;
             a = NumberUtils::normalizeDoublePrecision(normalizeAngle(a + step))) {

            gPrediction->determineShotResult(true, a);
            auto& cue = gPrediction->guiData.balls[0];
            if (!cue.onTable) continue;
            if (!gPrediction->guiData.collision.firstHitBall) continue;
            if (gPrediction->guiData.collision.firstHitBall->index != lowestIdx) continue;
            bool anyPotted = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& b = gPrediction->guiData.balls[i];
                if (b.originalOnTable && !b.onTable) { anyPotted = true; break; }
            }
            if (anyPotted) { AutoAim::setAimAngle(a); return; }
        }
    }

    // Called ONCE per turn — not every frame
    static void DoAIM() {
        if (!sharedGameManager.is9BallGame()) return;
        double base = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle());
        gPrediction->determineShotResult(true, base);
        int lowestIdx = findLowestBall();
        if (lowestIdx < 0) return;
        bool combo = false;
        if (lowestIdx < 9) combo = AIM_Combo(lowestIdx);
        if (!combo) AIM_Standard(lowestIdx);
    }

    // ── Visual button (play icon + thinking spinner) ──────────────────────────
    // Shown whenever bNineBallOneShoot is ON in a 9-ball game.
    // Positioned below the AutoPlay toggle button (or below the floating button
    // if AutoPlay button is not visible).
    static void DrawButton(ImGuiIO& io) {
        const float bsz    = 80.f;
        const float pad    = 6.0f;
        const float winW   = bsz + pad * 2.f;
        const float winH   = bsz + pad * 2.f;
        const float fbnSz  = 120.f; // floating button window size
        const float apWinH = 80.f + pad * 2.f; // autoplay toggle window height
        const float margin = 8.f;

        // Stack below autoplay button when it's visible
        float apOffset = persistent_bool[O("bAutoPlay")] ? (apWinH + margin) : 0.f;
        float posX = (g_btnX < 0.f) ? (io.DisplaySize.x - 20.f - winW) : g_btnX;
        float posY = (g_btnY < 0.f)
                     ? (io.DisplaySize.y - 20.f - winH)
                     : (g_btnY + fbnSz + margin + apOffset);
        posX = ImClamp(posX, 0.f, io.DisplaySize.x - winW);
        posY = ImClamp(posY, 0.f, io.DisplaySize.y - winH);

        SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
        SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
        PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
        PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(pad, pad));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 99.f);

        if (Begin(O("##NineBallBtn"), nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_NoMove)) {

            ImVec2  p      = GetCursorScreenPos();
            ImVec2  center(p.x + bsz * 0.5f, p.y + bsz * 0.5f);
            float   r      = bsz * 0.5f;
            ImDrawList* dl = GetWindowDrawList();

            InvisibleButton(O("##NBHit"), ImVec2(bsz, bsz));

            // Background circle
            ImU32 bgCol = bCalculated
                ? IM_COL32(10, 155, 60, 235)    // green = shot found
                : IM_COL32(18, 18, 45, 235);     // dark = thinking
            dl->AddCircleFilled(center, r, bgCol);

            // Outer ring
            ImU32 ringCol = bCalculated
                ? IM_COL32(0, 220, 90, 220)
                : IM_COL32(80, 100, 220, 160);
            dl->AddCircle(center, r - 2.f, ringCol, 48, 2.5f);

            // ── Thinking spinner (arc) ──────────────────────────────────────
            if (!bCalculated) {
                g_spinAngle += io.DeltaTime * 5.0f;
                const int segs   = 12;
                const float arcR = r - 10.f;
                for (int i = 0; i < segs; i++) {
                    float a0 = g_spinAngle + (float)i       / segs * (float)(M_PI * 1.6);
                    float a1 = g_spinAngle + (float)(i + 1) / segs * (float)(M_PI * 1.6);
                    int   al = (i * 230) / segs;
                    dl->PathArcTo(center, arcR, a0, a1, 3);
                    dl->PathStroke(IM_COL32(120, 180, 255, al), false, 3.0f);
                }
                // Small "9" in centre while thinking
                const char* lbl = "9";
                ImVec2 ts = CalcTextSize(lbl);
                dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
                            IM_COL32(200, 200, 255, 200), lbl);
            } else {
                // ── Shot found: play-triangle icon ─────────────────────────
                float th = r * 0.38f, tw = r * 0.34f;
                dl->AddTriangleFilled(
                    ImVec2(center.x - tw * 0.6f, center.y - th),
                    ImVec2(center.x - tw * 0.6f, center.y + th),
                    ImVec2(center.x + tw * 1.1f, center.y),
                    IM_COL32(255, 255, 255, 235));
            }
        }
        End();
        PopStyleVar(2);
        PopStyleColor(2);
    }

    // ── Main update — called from DrawESP, NOT from AutoPlay ─────────────────
    // Calculates aim ONCE per turn, then holds the result until turn ends.
    static void Update(ImDrawList* /*draw*/, ImGuiIO& io) {
        if (!persistent_bool[O("bNineBallOneShoot")]) return;
        if (!sharedGameManager || !sharedGameManager.is9BallGame()) return;

        auto sm = sharedGameManager.mStateManager();
        if (!sm) return;
        int stateId = sm.getCurrentStateId();

        // Draw button every frame so user always sees it
        DrawButton(io);

        if (stateId != 4) {
            // Not our turn — reset so we calculate fresh next turn
            if (g_prevStateId == 4) bCalculated = false;
            g_prevStateId = stateId;
            return;
        }
        g_prevStateId = stateId;

        // Calculate exactly once when turn starts
        if (!bCalculated) {
            DoAIM();
            bCalculated = true;
        }
        // Angle is now set — user shoots manually
    }

} // namespace NineBall

// ── AutoPlay::Update() ────────────────────────────────────────────────────────
namespace AutoPlay {
    static ImVec4 getPowerBarRect() {
        float hs = static_cast<float>(Height) / static_cast<float>(REF_HEIGHT);
        float ox = (static_cast<float>(Width) - hs * static_cast<float>(REF_WIDTH)) * 0.5f;
        float cx = ox + hs * 1120.0f;
        float bw = hs * 44.0f;
        float yt = hs * 162.0f;
        float yb = hs * 572.0f;
        return ImVec4(cx - bw * 0.5f, yt, bw, yb - yt);
    }

    static void Update() {
        if (!bAutoPlaying) return;

        // Tick the drag slider every single frame so the touch events fire correctly
        powerSlider.Update();

        // If a shot is already in progress, wait for it to complete
        if (powerSlider.Active) return;

        // Need a valid game state
        auto stateManager = sharedGameManager.mStateManager();
        if (!stateManager) return;
        int stateId = stateManager.getCurrentStateId();

        // Only act when it is our turn (stateId 4)
        if (stateId != 4) return;

        // Aim and shoot — standard AutoPlay (8-ball and 9-ball)
        AutoAim::AIM();
        powerSlider.SimulateDrag(getPowerBarRect(), SHOT_POWER, DRAG_TIME, HOLD_TIME);
    }
} // namespace AutoPlay


static void DrawContentArea(float winW, float winH) {
    bool need_save = false;
    
    ImDrawList* dl  = GetWindowDrawList();
    ImVec2      wp  = GetWindowPos();

    // startY este punctul unde se termină bara de butoane (Sidebar)
    float startY   = GetCursorPosY();
    float contentW = winW;

    // Desenăm fundalul zonei de conținut sub sidebar
    dl->AddRectFilled(
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + winH),
        IM_COL32(21, 21, 21, 255), 20.0f
    );
    
    const char* tabTitles[] = { 
    O("Draw Settings"), 
    O("Auto Play"), 
    O("Auto Queue"), 
    O("User"),
    O("Dump Lib")
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
        IM_COL32(60, 60, 75, 150), 1.0f
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
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
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
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
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
                PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.55f, 0.20f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.68f, 0.26f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.42f, 0.15f, 1.0f));
                if (Button(O("Save Config"), ImVec2(GetContentRegionAvail().x, 55.0f))) {
                    svConfig_Save();
                }
                PopStyleColor(3);
                PopStyleVar();
            }
            break;
        }
        
        case 1: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable AutoPlay"), &persistent_bool[O("bAutoPlay")]);

            Dummy(ImVec2(0, 14));
            need_save |= ToggleSwitch(O("9-Ball One Shoot"), &persistent_bool[O("bNineBallOneShoot")]);

            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("AutoPlay: auto aim & shoot"));
            Dummy(ImVec2(0, 6));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("9-Ball One Shoot: hit lowest"));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("ball first, combo 9-ball win"));
            break;
        }
        
        case 2: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable AutoQueue"), &persistent_bool[O("bAutoQueue")]);
            Dummy(ImVec2(0, 20));
            
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Mode"));
            Dummy(ImVec2(0, 8));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 12));
            PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= Combo("##mode", &persistent_int["iAutoQueue_Mode"], "Last Selected\0Smart\0Fix Table\0");
            PopStyleColor(2);
            PopStyleVar(2);
            
            if (persistent_int["iAutoQueue_Mode"] == 1) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Bet Percent"));
                Dummy(ImVec2(0, 8));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt("##betpercent", &persistent_int["iAutoQueue_BetPercent"], 1, 100, "%d%%");
                PopStyleColor(3);
                PopStyleVar(2);
            }

            if (persistent_int["iAutoQueue_Mode"] == 2) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Select Table"));
                Dummy(ImVec2(0, 8));

                struct TableEntry { const char* label; ImU32 bg; ImU32 bgHov; };
                static const TableEntry tables[17] = {
                    { "100",   IM_COL32( 55,  90, 200, 255), IM_COL32( 75, 110, 220, 255) }, // M1  Blue
                    { "200",   IM_COL32( 40, 150,  65, 255), IM_COL32( 55, 170,  80, 255) }, // M2  Green
                    { "1k",    IM_COL32( 55,  90, 200, 255), IM_COL32( 75, 110, 220, 255) }, // M3  Blue
                    { "2.5k",  IM_COL32(130,  25,  25, 255), IM_COL32(155,  40,  40, 255) }, // M4  Dark Red
                    { "10k",   IM_COL32( 35,  35,  38, 255), IM_COL32( 55,  55,  60, 255) }, // M5  Black
                    { "50k",   IM_COL32(110,   0,   0, 255), IM_COL32(135,  15,  15, 255) }, // M6  Maroon
                    { "100k",  IM_COL32(140, 140, 145, 255), IM_COL32(160, 160, 165, 255) }, // M7  Light Grey
                    { "500k",  IM_COL32(185, 160,   0, 255), IM_COL32(210, 185,  10, 255) }, // M8  Yellow
                    { "1M",    IM_COL32( 20,  45, 130, 255), IM_COL32( 35,  60, 155, 255) }, // M9  Dark Blue
                    { "2M",    IM_COL32(190,  90,  15, 255), IM_COL32(215, 110,  30, 255) }, // M10 Dark Orange
                    { "5M",    IM_COL32(  0, 148, 110, 255), IM_COL32( 15, 170, 128, 255) }, // M11 Emerald
                    { "8M",    IM_COL32(165,  65,  65, 255), IM_COL32(185,  85,  85, 255) }, // M12 Light Maroon
                    { "10M",   IM_COL32( 18,  90,  35, 255), IM_COL32( 30, 112,  50, 255) }, // M13 Dark Green
                    { "20M",   IM_COL32(100, 100, 110, 255), IM_COL32(120, 120, 130, 255) }, // M14 Grey
                    { "30M",   IM_COL32(130,  15,  35, 255), IM_COL32(155,  30,  50, 255) }, // M15 Red Maroon
                    { "50M",   IM_COL32(  0, 148, 110, 255), IM_COL32( 15, 170, 128, 255) }, // M16 Emerald
                    { "200M",  IM_COL32( 20,  45, 130, 255), IM_COL32( 35,  60, 155, 255) }, // M17 Dark Blue
                };

                int& selected = persistent_int["iAutoQueue_FixTable"];
                float avail   = GetContentRegionAvail().x;
                int   cols    = 4;
                float gap     = 8.0f;
                float btnW    = (avail - gap * (cols - 1)) / cols;
                float btnH    = 42.0f;

                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));

                for (int i = 0; i < 17; i++) {
                    if (i % cols != 0) SameLine(0, gap);

                    bool isSel = (selected == i);
                    ImU32 bgCol = isSel ? tables[i].bgHov : tables[i].bg;

                    PushStyleColor(ImGuiCol_Button,        (ImU32)bgCol);
                    PushStyleColor(ImGuiCol_ButtonHovered, (ImU32)tables[i].bgHov);
                    PushStyleColor(ImGuiCol_ButtonActive,  (ImU32)tables[i].bgHov);
                    PushStyleColor(ImGuiCol_Text,          isSel ? IM_COL32(255,255,255,255) : IM_COL32(220,220,220,200));

                    char btnId[32];
                    snprintf(btnId, sizeof(btnId), "%s##ft%d", tables[i].label, i);
                    if (Button(btnId, ImVec2(btnW, btnH))) {
                        selected = i;
                        need_save = true;
                    }

                    // Selected indicator: white outline
                    if (isSel) {
                        ImVec2 p = GetItemRectMin();
                        ImVec2 q = GetItemRectMax();
                        GetWindowDrawList()->AddRect(p, q, IM_COL32(255,255,255,200), 10.0f, 0, 2.0f);
                    }

                    PopStyleColor(4);
                }

                PopStyleVar(2);
            }

            if (persistent_int["iAutoQueue_Mode"] == 0) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("You will be auto queued to"));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("the last game mode you played"));
            }
            break;
        }

        case 3: {
            // ── helpers ──────────────────────────────────────────────────────
            auto DrawSectionHeader = [&](const char* title) {
                Dummy(ImVec2(0, 14));
                float avail = GetContentRegionAvail().x;
                ImVec2 p    = GetCursorScreenPos();
                float  fs   = GImGui->FontSize;
                ImVec2 ts   = CalcTextSize(title);
                float  lineY = p.y + fs * 0.5f;
                float  gap   = 8.0f;
                float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
                ImDrawList* dl2 = GetWindowDrawList();
                dl2->AddLine(ImVec2(p.x,                      lineY), ImVec2(p.x + lineW,                      lineY), IM_COL32(60,60,75,160), 1.0f);
                dl2->AddLine(ImVec2(p.x + lineW + gap + ts.x + gap, lineY), ImVec2(p.x + avail, lineY), IM_COL32(60,60,75,160), 1.0f);
                SetCursorPosX(GetCursorPosX() + lineW + gap);
                TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", title);
                Dummy(ImVec2(0, 6));
            };

            auto DrawInfoRow = [&](const char* key, const char* val) {
                TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", key);
                SameLine();
                TextColored(ImVec4(0.90f, 0.90f, 0.95f, 1.0f), "%s", val);
                Dummy(ImVec2(0, 4));
            };

          /*  // ── User Game Info ────────────────────────────────────────────────
            DrawSectionHeader(O("User Game Info"));

            if (sharedUserInfo) {
                DrawInfoRow(O("Coins:        "), ReadNSString(sharedUserInfo.coins()).c_str());
                DrawInfoRow(O("Cash:         "), ReadNSString(sharedUserInfo.cash()).c_str());
                DrawInfoRow(O("Display Name: "), ReadNSString(sharedUserInfo.DisplayName()).c_str());
                DrawInfoRow(O("Country Code: "), ReadNSString(sharedUserInfo.loginCountryCode()).c_str());
            } else {
                TextColored(ImVec4(0.6f, 0.3f, 0.3f, 1.0f), O("UserInfo not available"));
            }*/

            // ── Device Info ───────────────────────────────────────────────────
            DrawSectionHeader(O("Device"));
            {
                static char s_manufacturer[PROP_VALUE_MAX] = {};
                static char s_model[PROP_VALUE_MAX]        = {};
                static char s_abi[PROP_VALUE_MAX]          = {};
                static char s_android[PROP_VALUE_MAX]      = {};
                static bool s_props_loaded = false;
                if (!s_props_loaded) {
                    __system_property_get("ro.product.manufacturer", s_manufacturer);
                    __system_property_get("ro.product.model",        s_model);
                    __system_property_get("ro.product.cpu.abi",      s_abi);
                    __system_property_get("ro.build.version.release", s_android);
                    s_props_loaded = true;
                }
                DrawInfoRow(O("Manufacturer: "), s_manufacturer);
                DrawInfoRow(O("Model:        "), s_model);
                DrawInfoRow(O("ABI:          "), s_abi);
                DrawInfoRow(O("Android:      "), s_android);
            }

            // ── Red Engine Info ───────────────────────────────────────────────
            DrawSectionHeader(O("Your  Info"));
            {
                int64_t now_ts   = (int64_t)time(nullptr);
                int64_t diff     = EXPIRY_TS - now_ts;

                char expireBuf[64];
                if (diff > 0) {
                    int64_t totalSecs = diff;
                    int days  = (int)(totalSecs / 86400);
                    int hours = (int)((totalSecs % 86400) / 3600);
                    int mins  = (int)((totalSecs % 3600)  / 60);
                    snprintf(expireBuf, sizeof(expireBuf), "%dd - %dh - %dm", days, hours, mins);
                } else {
                    snprintf(expireBuf, sizeof(expireBuf), "%s", O("Expired"));
                }

                DrawInfoRow(O("Expire:       "), expireBuf);
                Dummy(ImVec2(0, 6));
                TextColored(ImVec4(0, 1.f, 0, 1.0f), O("Powered By @Kz.tutorial"));
                Dummy(ImVec2(0, 8));
                PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
                TextColored(ImVec4(1.f, 0.f, 0.f, 1.0f),
                    O("Beware of Scammers. This is a FREE BETA version, if you bought this version it means you got scammed."));
                PopTextWrapPos();
            }
            break;
        }
        case 4: {
            DumpLib::DrawUI();
            break;
        }
    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleColor();
}

INLINE void DrawMenu(ImGuiIO& io) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        // 9-Ball One Shoot: manual aim helper with button + thinking animation.
        // Called here (after NineBall namespace is defined) to avoid forward-decl issues.
        NineBall::Update(nullptr, io);

        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 14.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarWidth * sizeScale;
            float winH = 560.0f * sizeScale;

            // Initialise to screen centre on first open
            if (g_menuPosX < 0.0f) g_menuPosX = (Width  - winW) * 0.5f;
            if (g_menuPosY < 0.0f) g_menuPosY = (Height - winH) * 0.5f;

            // Clamp to screen every frame so it never leaves the display
            g_menuPosX = ImClamp(g_menuPosX, 0.0f, ImMax(0.0f, io.DisplaySize.x - winW));
            g_menuPosY = ImClamp(g_menuPosY, 0.0f, ImMax(0.0f, io.DisplaySize.y - winH));

            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(g_menuPosX, g_menuPosY), ImGuiCond_Always);

            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.13f, 0.f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);

            // NoMove kept — we handle dragging manually so we can clamp
            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

            if (Begin(O("##MainMenu"), &g_menu.isOpen, winFlags)) {
                // ── Draw border glow ──────────────────────────────────────────
                ImDrawList* bdl = GetWindowDrawList();
                ImVec2      bwp = GetWindowPos();
                bdl->AddRect(bwp, ImVec2(bwp.x + winW, bwp.y + winH),
                             IM_COL32(200, 30, 30, 60), 20.0f, 0, 1.5f);
                bdl->AddRect(ImVec2(bwp.x + 1, bwp.y + 1),
                             ImVec2(bwp.x + winW - 1, bwp.y + winH - 1),
                             IM_COL32(255, 60, 60, 25), 20.0f, 0, 1.0f);

                DrawSidebar(winW);
                DrawContentArea(winW, winH);

                // ── Drag: any area of the window not captured by a widget ────
                if (IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
                    IsMouseDragging(ImGuiMouseButton_Left, 4.0f) &&
                    !IsAnyItemActive()) {
                    g_menuPosX += io.MouseDelta.x;
                    g_menuPosY += io.MouseDelta.y;
                    g_menuPosX = ImClamp(g_menuPosX, 0.0f, ImMax(0.0f, io.DisplaySize.x - winW));
                    g_menuPosY = ImClamp(g_menuPosY, 0.0f, ImMax(0.0f, io.DisplaySize.y - winH));
                    SetWindowPos(ImVec2(g_menuPosX, g_menuPosY), ImGuiCond_Always);
                }
            }
            End();

            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

// Moved from AutoPlay namespace — plays/pauses autoplay (cancelMode=false)
// or cancels autoqueue (cancelMode=true)
static void DrawToggleButton(bool cancelMode) {
    ImGuiIO& io = GetIO();

    float button_size  = 80.f;
    float winPad       = 6.0f;
    float windowWidth  = button_size + winPad * 2.0f;
    float windowHeight = button_size + winPad * 2.0f;
    float margin       = 8.0f;

    // Position the AutoPlay/Cancel button just below the floating button
    // g_btnX/g_btnY tracks the floating button position
    float floatBtnSize = 140.0f; // floating button window size
    float posX = (g_btnX < 0.0f) ? (io.DisplaySize.x - 20.0f - windowWidth) : g_btnX;
    float posY = (g_btnY < 0.0f) ? (io.DisplaySize.y - 20.0f - windowHeight)
                                  : (g_btnY + floatBtnSize + margin);

    // Clamp so it never goes off screen
    posX = ImClamp(posX, 0.0f, io.DisplaySize.x - windowWidth);
    posY = ImClamp(posY, 0.0f, io.DisplaySize.y - windowHeight);

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(winPad, winPad));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 99.0f);

    if (Begin(O("##ToggleBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {

        ImVec2 pos    = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        float  r      = size.x * 0.5f;

        bool clicked = InvisibleButton(O("##TglBtnHit"), size);
        bool hovered = IsItemHovered();

        ImDrawList* dl = GetWindowDrawList();

        // Determine current state for visual
        bool isOn = cancelMode ? true : AutoPlay::bAutoPlaying;
        int  stateId = 0;
        if (!cancelMode && sharedGameManager && sharedGameManager.mStateManager()) {
            stateId = sharedGameManager.mStateManager().getCurrentStateId();
        }
        bool myTurn = (!cancelMode && stateId == 4);

        // Outer circle background
        ImU32 bgCol;
        if (cancelMode)      bgCol = IM_COL32(180, 40, 40, 220);
        else if (!isOn)      bgCol = IM_COL32(40, 40, 45, 200);
        else if (myTurn)     bgCol = IM_COL32(0, 180, 60, 230);
        else                 bgCol = IM_COL32(220, 30, 30, 220);

        ImU32 ringCol = hovered ? IM_COL32(255,255,255,160) : IM_COL32(255,255,255,60);
        dl->AddCircleFilled(center, r, bgCol);
        dl->AddCircle(center, r - 2.f, ringCol, 48, 2.f);

        // Icon inside: play triangle, stop bars, or X
        if (cancelMode) {
            // X symbol for cancel
            float xh = r * 0.35f;
            ImU32 xc = IM_COL32(255,255,255,240);
            dl->AddLine(ImVec2(center.x-xh, center.y-xh), ImVec2(center.x+xh, center.y+xh), xc, 3.5f);
            dl->AddLine(ImVec2(center.x+xh, center.y-xh), ImVec2(center.x-xh, center.y+xh), xc, 3.5f);
        } else if (!isOn) {
            // Play triangle (pointing right)
            float th = r * 0.38f;
            float tw = r * 0.34f;
            ImVec2 p1(center.x - tw * 0.6f, center.y - th);
            ImVec2 p2(center.x - tw * 0.6f, center.y + th);
            ImVec2 p3(center.x + tw * 1.1f, center.y);
            dl->AddTriangleFilled(p1, p2, p3, IM_COL32(255,255,255,230));
        } else {
            // Stop/pause bars when playing
            float bh = r * 0.38f;
            float bw = r * 0.22f;
            float gap = r * 0.15f;
            ImU32 ic = IM_COL32(255,255,255,230);
            dl->AddRectFilled(ImVec2(center.x - gap - bw, center.y - bh),
                              ImVec2(center.x - gap,       center.y + bh), ic, 3.f);
            dl->AddRectFilled(ImVec2(center.x + gap,       center.y - bh),
                              ImVec2(center.x + gap + bw,  center.y + bh), ic, 3.f);
        }

        if (clicked) {
            if (cancelMode) {
                persistent_bool[O("bAutoQueue")] = false;
                g_aqCounting = false;
            } else {
                AutoPlay::bAutoPlaying = !AutoPlay::bAutoPlaying;
                if (AutoPlay::bAutoPlaying) AutoPlay::ClearState();
            }
        }
    }
    End();
    PopStyleVar(2);
    PopStyleColor(2);
}

static void DrawFloatingButton(ImGuiIO& io) {
    static bool isDragging = false;

    // Button is a circle of radius 55px inside a 120×120 window
    const float r       = 55.0f;
    const float winSize = 120.0f;

    // Initialise to bottom-right on first frame
    if (g_btnX < 0.0f) g_btnX = io.DisplaySize.x - winSize - 16.0f;
    if (g_btnY < 0.0f) g_btnY = io.DisplaySize.y - winSize - 24.0f;

    // Hard clamp every frame — button never leaves screen
    g_btnX = ImClamp(g_btnX, 0.0f, io.DisplaySize.x - winSize);
    g_btnY = ImClamp(g_btnY, 0.0f, io.DisplaySize.y - winSize);

    SetNextWindowPos(ImVec2(g_btnX, g_btnY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(winSize, winSize), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize    |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
              ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoSavedSettings)) {

        ImVec2      wp     = GetWindowPos();
        ImVec2      center = ImVec2(wp.x + winSize * 0.5f, wp.y + winSize * 0.5f);
        ImDrawList* dl     = GetWindowDrawList();

        SetCursorPos(ImVec2(0, 0));
        bool clicked = InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));
        bool hovered = IsItemHovered();

        // ── Background circle (dark with subtle depth rings) ─────────────
        dl->AddCircleFilled(center, r + 3.0f, IM_COL32(0, 0, 0, 60));  // shadow
        dl->AddCircleFilled(center, r,         IM_COL32(14, 14, 26, 248));

        // Red glow rings — brighter when hovered / menu open
        ImU32 ringAlpha = (hovered || g_menu.isOpen) ? 230u : 140u;
        dl->AddCircle(center, r - 1.0f,        IM_COL32(220, 35,  35, ringAlpha), 48, 2.5f);
        dl->AddCircle(center, r - 6.0f,        IM_COL32(160, 30,  30, ringAlpha / 3), 48, 1.0f);

        // ── Hamburger icon (3 horizontal lines) ──────────────────────────
        float lw  = r * 0.54f;   // full line half-width
        float lsp = r * 0.31f;   // spacing between lines
        float lth = 3.2f;        // line thickness
        ImU32 ic  = IM_COL32(255, 255, 255, 228);
        for (int i = -1; i <= 1; i++) {
            float ly   = center.y + i * lsp;
            float half = (i == 0) ? lw * 0.70f : lw;  // middle line shorter
            dl->AddLine(ImVec2(center.x - half, ly),
                        ImVec2(center.x + half, ly), ic, lth);
        }

        // ── Active glow when menu is open ─────────────────────────────────
        if (g_menu.isOpen) {
            dl->AddCircle(center, r + 1.5f, IM_COL32(255, 80, 80, 50), 48, 4.0f);
        }

        // ── X+Y free drag ─────────────────────────────────────────────────
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            isDragging = true;
            g_btnX += io.MouseDelta.x;
            g_btnY += io.MouseDelta.y;
            g_btnX = ImClamp(g_btnX, 0.0f, io.DisplaySize.x - winSize);
            g_btnY = ImClamp(g_btnY, 0.0f, io.DisplaySize.y - winSize);
            SetWindowPos(ImVec2(g_btnX, g_btnY), ImGuiCond_Always);
        }

        if (clicked && !isDragging) g_menu.isOpen = !g_menu.isOpen;
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
    ImVec2 titleSize = CalcTextSize(O("Your Name"));
    dl->AddText(ImVec2(winPos.x + (cardW - titleSize.x) * 0.5f, winPos.y + 30), IM_COL32(255, 255, 255, 255), O("Your Name"));
    SetWindowFontScale(1.0f);
    
    ImVec2 subSize = CalcTextSize(O("Premium Mod"));
    dl->AddText(ImVec2(winPos.x + (cardW - subSize.x) * 0.5f, winPos.y + 70), IM_COL32(200, 220, 255, 200), O("Premium Mod"));

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
    
    TextColored(ImColor(0, 255, 0, 255), O("Powered By @Kz.tutorial"));
    
    End();
}

        if (g_autoPlayCalculating) DrawCalculating(io);
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
