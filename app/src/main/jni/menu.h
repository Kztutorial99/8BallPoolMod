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
    bool isOpen      = false;
    int  currentTab  = 0;
    float sidebarWidth = 750.0f;
    float animProgress = 0.0f;
    float menuAlpha    = 0.0f;
    float menuScale    = 0.9f;
    ImVec4 accentColor = ImVec4(0.85f, 0.12f, 0.18f, 1.0f);
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

// iconType: 0=eye(Visual) 1=crosshair(Aim) 2=wrench(Misc) 3=person(User)
static void DrawTabIcon(ImDrawList* dl, ImVec2 center, float sz, int iconType, bool selected) {
    ImU32 col  = selected ? IM_COL32(255, 255, 255, 245) : IM_COL32(160, 175, 210, 200);
    ImU32 col2 = selected ? IM_COL32(160, 210, 255, 200) : IM_COL32(100, 115, 150, 150);
    float lw   = selected ? 2.0f : 1.5f;

    if (iconType == 0) {
        // Eye icon — Visual
        float ew = sz * 0.40f;
        float eh = sz * 0.20f;
        const int N = 24;
        ImVec2 pts[N];
        for (int i = 0; i < N; i++) {
            float a = (float)i / N * 3.14159f * 2.0f;
            pts[i] = ImVec2(center.x + cosf(a) * ew, center.y + sinf(a) * eh);
        }
        dl->AddPolyline(pts, N, col, ImDrawFlags_Closed, lw);
        dl->AddCircleFilled(center, sz * 0.12f, col, 16);
        dl->AddCircleFilled(ImVec2(center.x + sz * 0.06f, center.y - sz * 0.06f),
                            sz * 0.04f, IM_COL32(255,255,255,180));
    } else if (iconType == 1) {
        // Crosshair icon — Aim
        float cr  = sz * 0.34f;
        float gap = sz * 0.11f;
        dl->AddCircle(center, cr * 0.55f, col, 24, lw);
        dl->AddLine(ImVec2(center.x - cr, center.y),  ImVec2(center.x - gap, center.y), col, lw);
        dl->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + cr,  center.y), col, lw);
        dl->AddLine(ImVec2(center.x, center.y - cr),  ImVec2(center.x, center.y - gap), col, lw);
        dl->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + cr),  col, lw);
        dl->AddCircleFilled(center, sz * 0.05f, col);
    } else if (iconType == 2) {
        // Wrench icon — Misc
        float wl = sz * 0.35f;
        ImVec2 pA(center.x - wl * 0.60f, center.y + wl * 0.60f);
        ImVec2 pB(center.x + wl * 0.50f, center.y - wl * 0.50f);
        dl->AddLine(pA, pB, col, sz * 0.16f);
        dl->AddCircle(ImVec2(pB.x + sz * 0.06f, pB.y - sz * 0.06f),
                      sz * 0.17f, col, 16, lw);
        dl->AddCircleFilled(ImVec2(pA.x - sz * 0.05f, pA.y + sz * 0.05f),
                            sz * 0.09f, col2);
    } else {
        // Person icon — User
        // Head
        dl->AddCircle(ImVec2(center.x, center.y - sz * 0.20f), sz * 0.14f, col, 20, lw);
        // Shoulders arc (half-ellipse)
        const int SN = 12;
        ImVec2 spts[SN];
        float sr = sz * 0.28f;
        for (int i = 0; i < SN; i++) {
            float a = 3.14159f + (float)i / (SN - 1) * 3.14159f;
            spts[i] = ImVec2(center.x + cosf(a) * sr,
                             center.y + sz * 0.16f + sinf(a) * sr * 0.45f);
        }
        dl->AddPolyline(spts, SN, col, ImDrawFlags_None, lw);
    }
}

// ── Vertical Tab Button — untuk sidebar kiri ─────────────────────────────────
static bool VerticalTabButton(const char* label, int iconType, bool selected,
                              float width, float height) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height);
    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    float iconSz = 34.0f;
    ImVec2 iconCenter = ImVec2(bb.Min.x + width * 0.5f,
                               bb.Min.y + height * 0.40f);

    if (selected) {
        // Left accent bar — ice blue
        dl->AddRectFilled(
            ImVec2(bb.Min.x, bb.Min.y + height * 0.14f),
            ImVec2(bb.Min.x + 3.2f, bb.Max.y - height * 0.14f),
            IM_COL32(0, 185, 225, 210), 2.0f);
        // Glass highlight bg
        dl->AddRectFilledMultiColor(
            bb.Min, bb.Max,
            IM_COL32(0, 130, 175, 28), IM_COL32(0, 110, 155, 28),
            IM_COL32(0,  90, 135, 12), IM_COL32(0,  70, 115, 12));
        // Top sheen
        dl->AddRectFilledMultiColor(
            bb.Min, ImVec2(bb.Max.x, bb.Min.y + height * 0.35f),
            IM_COL32(255,255,255,16), IM_COL32(255,255,255,16),
            IM_COL32(255,255,255, 0), IM_COL32(255,255,255, 0));
        // Bottom micro-glow line
        dl->AddRectFilled(
            ImVec2(bb.Min.x + width * 0.25f, bb.Max.y - 1.0f),
            ImVec2(bb.Max.x - width * 0.25f, bb.Max.y),
            IM_COL32(0, 175, 215, 55));
    } else if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(255, 255, 255, 7));
        dl->AddRect(bb.Min, bb.Max, IM_COL32(0, 140, 180, 18), 0, 0, 0.6f);
    }

    // Separator line between tabs (except last)
    dl->AddRectFilled(
        ImVec2(bb.Min.x + 12.0f, bb.Max.y - 0.6f),
        ImVec2(bb.Max.x - 12.0f, bb.Max.y),
        IM_COL32(255, 255, 255, 8));

    // Icon
    DrawTabIcon(dl, iconCenter, iconSz, iconType, selected);

    // Label
    float fSzSmall = GImGui->FontSize * 0.70f;
    ImVec2 labelSz = GImGui->Font->CalcTextSizeA(fSzSmall, FLT_MAX, 0, label);
    ImVec2 textPos = ImVec2(
        bb.Min.x + (width - labelSz.x) * 0.5f,
        bb.Min.y + height * 0.69f);
    ImU32 textCol = selected
        ? IM_COL32(120, 205, 255, 255)
        : IM_COL32(72, 105, 140, 195);
    dl->AddText(GImGui->Font, fSzSmall, textPos, textCol, label);

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

    // Left accent bar when ON — red
    if (animT > 0.05f) {
        float barH = size.y * 0.55f;
        float barY = bb.Min.y + (size.y - barH) * 0.5f;
        dl->AddRectFilled(
            ImVec2(bb.Min.x, barY),
            ImVec2(bb.Min.x + 2.5f, barY + barH),
            IM_COL32(220, 40, 60, (int)(animT * 210)), 2.0f);
    }

    // Toggle track
    ImVec2 togglePos = ImVec2(bb.Max.x - width - 14.0f, bb.Min.y + (rowH - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + width, togglePos.y + height);

    ImVec4 offColor = ImVec4(0.10f, 0.14f, 0.22f, 1.0f);
    ImVec4 onColor  = ImVec4(0.05f, 0.58f, 0.78f, 1.0f);
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
#include "game/inc/EnemySabotage.h"
#include "game/inc/GhostMode.h"
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

// ── Sidebar Kiri Vertikal — Smooth Dark Glass ─────────────────────────────────
static void DrawSidebar(float sidebarW, float winH) {
    ImVec2 csp = GetCursorScreenPos();
    ImDrawList* dl = GetWindowDrawList();

    // ── Background panel (drawn before child for correct layering) ────────
    dl->AddRectFilledMultiColor(
        csp, ImVec2(csp.x + sidebarW, csp.y + winH),
        IM_COL32( 5,  8, 18, 255), IM_COL32( 9, 13, 24, 255),
        IM_COL32( 7, 11, 21, 255), IM_COL32( 4,  7, 15, 255));
    // Top gloss sheen
    dl->AddRectFilledMultiColor(
        csp, ImVec2(csp.x + sidebarW, csp.y + winH * 0.28f),
        IM_COL32(255,255,255,16), IM_COL32(255,255,255,16),
        IM_COL32(255,255,255, 0), IM_COL32(255,255,255, 0));
    // Bottom darkening
    dl->AddRectFilledMultiColor(
        ImVec2(csp.x, csp.y + winH * 0.68f),
        ImVec2(csp.x + sidebarW, csp.y + winH),
        IM_COL32(0,0,0, 0), IM_COL32(0,0,0, 0),
        IM_COL32(0,0,0,45), IM_COL32(0,0,0,45));
    // Right edge separator — ice blue glow
    dl->AddRectFilledMultiColor(
        ImVec2(csp.x + sidebarW - 1.5f, csp.y),
        ImVec2(csp.x + sidebarW,        csp.y + winH),
        IM_COL32(0, 155, 200,  0), IM_COL32(0, 155, 200, 95),
        IM_COL32(0, 155, 200, 95), IM_COL32(0, 155, 200,  0));

    // ── Child window for interactive tab buttons ───────────────────────────
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    BeginChild(O("##VSidebar"), ImVec2(sidebarW, winH), false,
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* sdl = GetWindowDrawList();
    ImVec2      swp = GetWindowPos();

    // ── Header mod name ───────────────────────────────────────────────────
    float hdrH = 60.0f;

    // Header bg strip
    sdl->AddRectFilledMultiColor(
        swp, ImVec2(swp.x + sidebarW, swp.y + hdrH),
        IM_COL32( 4,  7, 18, 255), IM_COL32( 8, 12, 25, 255),
        IM_COL32( 6, 10, 22, 255), IM_COL32( 3,  6, 16, 255));
    // Header bottom accent line
    sdl->AddRectFilledMultiColor(
        ImVec2(swp.x + 4.0f, swp.y + hdrH - 1.2f),
        ImVec2(swp.x + sidebarW - 4.0f, swp.y + hdrH),
        IM_COL32(0,120,170, 0), IM_COL32(0,185,225,120),
        IM_COL32(0,185,225,120), IM_COL32(0,120,170, 0));

    // "FLUX" brand text — plain strings (no obfuscation, these are visible UI labels)
    const char* brandA = "FLUX";
    const char* brandB = "PRO";
    SetWindowFontScale(1.20f);
    ImVec2 szA = CalcTextSize(brandA);
    SetCursorPos(ImVec2((sidebarW - szA.x) * 0.5f, 9.0f));
    TextColored(ImVec4(0.42f, 0.82f, 1.0f, 1.0f), "%s", brandA);
    SetWindowFontScale(0.68f);
    ImVec2 szB = CalcTextSize(brandB);
    SetCursorPos(ImVec2((sidebarW - szB.x) * 0.5f, 9.0f + szA.y * 1.20f - 1.0f));
    TextColored(ImVec4(0.28f, 0.58f, 0.80f, 0.82f), "%s", brandB);
    SetWindowFontScale(1.0f);

    // ── 4 Vertical Tab Buttons ────────────────────────────────────────────
    float tabAvailH = winH - hdrH;
    float tabH      = tabAvailH / 4.0f;

    // Plain string labels — O() macro TIDAK boleh dipakai untuk display text
    // karena hasilnya bisa berupa pointer ke buffer yang sama (ID collision)
    // dan karakter obfuscated menyebabkan teks tampil sebagai "???"
    struct { const char* lbl; int icon; } tabs[4] = {
        { "Visual", 0 },
        { "Aim",    1 },
        { "Misc",   2 },
        { "User",   3 },
    };
    for (int i = 0; i < 4; i++) {
        SetCursorPos(ImVec2(0.0f, hdrH + i * tabH));
        // PushID(i) memastikan setiap tab punya ImGui ID unik
        // sehingga semua tab bisa diklik (fix: 8 Ball tab tidak bisa diklik)
        PushID(i);
        if (VerticalTabButton(tabs[i].lbl, tabs[i].icon,
                              g_menu.currentTab == i, sidebarW, tabH))
            g_menu.currentTab = i;
        PopID();
    }

    EndChild();
    PopStyleColor();
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


static void DrawContentArea(float contentW, float winH) {
    bool need_save = false;

    ImDrawList* dl = GetWindowDrawList();
    ImVec2      wp = GetWindowPos();

    // Position of this content panel (right of sidebar)
    float startX = GetCursorPosX();
    float startY = GetCursorPosY();   // 0 in vertical layout

    // ── Smooth Dark Glass background ──────────────────────────────────────
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX, wp.y + startY),
        ImVec2(wp.x + startX + contentW, wp.y + winH),
        IM_COL32(8, 11, 22, 252), IM_COL32(10, 14, 26, 252),
        IM_COL32(9,  13, 24, 252), IM_COL32(7,  10, 20, 252));
    // Left subtle ice-blue edge
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX,        wp.y + startY),
        ImVec2(wp.x + startX + 2.0f, wp.y + winH),
        IM_COL32(0,155,200,38), IM_COL32(0,155,200,38),
        IM_COL32(0,155,200, 8), IM_COL32(0,155,200, 8));
    // Right subtle ice-blue edge
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX + contentW - 2.0f, wp.y + startY),
        ImVec2(wp.x + startX + contentW,        wp.y + winH),
        IM_COL32(0,155,200,38), IM_COL32(0,155,200,38),
        IM_COL32(0,155,200, 8), IM_COL32(0,155,200, 8));
    // Bottom vignette
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX, wp.y + winH * 0.78f),
        ImVec2(wp.x + startX + contentW, wp.y + winH),
        IM_COL32(0,0,0, 0), IM_COL32(0,0,0, 0),
        IM_COL32(0,0,0,55), IM_COL32(0,0,0,55));

    static const char* const tabTitles[]    = { "VISUAL", "AIM", "MISC", "USER" };
    static const char* const tabSubtitles[] = {
        "ESP and draw settings",
        "Aim mode selection",
        "Ghost mode & extras",
        "Mod info & device",
    };
    const ImU32 tabAccents[] = {
        IM_COL32( 0, 185, 225, 215),
        IM_COL32(10, 170, 210, 215),
        IM_COL32(20, 195, 230, 215),
        IM_COL32( 0, 160, 200, 215),
    };

    const char* currentTitle    = tabTitles[g_menu.currentTab];
    const char* currentSubtitle = tabSubtitles[g_menu.currentTab];
    ImU32 accentCol = tabAccents[g_menu.currentTab];

    float titlePadT = 10.0f;
    float titlePadB = 8.0f;

    // Measure title + subtitle height
    SetWindowFontScale(1.10f);
    ImVec2 ts = CalcTextSize(currentTitle);
    SetWindowFontScale(0.78f);
    ImVec2 ss = CalcTextSize(currentSubtitle);
    SetWindowFontScale(1.0f);

    float titleBlockH = titlePadT + ts.y + 4.0f + ss.y + titlePadB;

    // Title background strip
    dl->AddRectFilled(
        ImVec2(wp.x + startX, wp.y + startY),
        ImVec2(wp.x + startX + contentW, wp.y + startY + titleBlockH),
        IM_COL32(6, 10, 22, 220));

    // Left accent stripe
    dl->AddRectFilled(
        ImVec2(wp.x + startX, wp.y + startY + titlePadT * 0.4f),
        ImVec2(wp.x + startX + 3.5f, wp.y + startY + titleBlockH - titlePadT * 0.4f),
        accentCol, 2.0f);

    // FPS bar — top right
    {
        float fps = ImGui::GetIO().Framerate;
        char fpsBuf[48];
        snprintf(fpsBuf, sizeof(fpsBuf), "FPS %.0f", fps);
        SetWindowFontScale(0.72f);
        ImVec2 fpsSz = CalcTextSize(fpsBuf);
        dl->AddText(GImGui->Font, GImGui->FontSize * 0.72f,
            ImVec2(wp.x + startX + contentW - fpsSz.x - 10.0f,
                   wp.y + startY + titlePadT + (ts.y - fpsSz.y) * 0.5f),
            IM_COL32(100, 130, 160, 180), fpsBuf);
        SetWindowFontScale(1.0f);
    }

    // Title text (left-aligned with small pad)
    float titleX = startX + 14.0f;
    SetWindowFontScale(1.10f);
    SetCursorPos(ImVec2(titleX, startY + titlePadT));
    TextColored(ImVec4(0.88f, 0.95f, 1.0f, 1.0f), "%s", currentTitle);
    SetWindowFontScale(0.78f);
    // Separator pipe
    SameLine(0, 8.0f);
    TextColored(ImVec4(0.40f, 0.55f, 0.70f, 1.0f), "|");
    SameLine(0, 8.0f);
    TextColored(ImVec4(0.55f, 0.65f, 0.80f, 1.0f), "%s", currentSubtitle);
    SetWindowFontScale(1.0f);

    // Separator line — ice blue gradient
    float lineY = startY + titleBlockH;
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX,                    wp.y + lineY - 0.5f),
        ImVec2(wp.x + startX + contentW * 0.35f,  wp.y + lineY + 1.0f),
        IM_COL32(0,0,0,0), accentCol, accentCol, IM_COL32(0,0,0,0));
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x + startX + contentW * 0.35f,  wp.y + lineY - 0.5f),
        ImVec2(wp.x + startX + contentW,           wp.y + lineY + 1.0f),
        accentCol, IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), accentCol);

    float headerH = lineY - startY + 8.0f;
    SetCursorPos(ImVec2(startX + 10.0f, startY + headerH));

    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false);
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 8));

            // ── Section header helper ────────────────────────────────────────
            auto DrawSectionHdr0 = [&](const char* title) {
                Dummy(ImVec2(0, 4));
                TextColored(ImVec4(0.52f, 0.68f, 0.88f, 0.90f), "%s", title);
                Dummy(ImVec2(0, 3));
                ImVec2 lp = GetCursorScreenPos();
                GetWindowDrawList()->AddLine(lp,
                    ImVec2(lp.x + GetContentRegionAvail().x, lp.y),
                    IM_COL32(45, 75, 120, 100), 1.0f);
                Dummy(ImVec2(0, 7));
            };

            // ── Checkbox style helper ────────────────────────────────────────
            auto StyledCheckbox = [&](const char* label, bool* val) -> bool {
                PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.13f, 0.15f, 0.22f, 1.0f));
                PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.18f, 0.22f, 0.32f, 1.0f));
                PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.10f, 0.38f, 0.78f, 1.0f));
                PushStyleColor(ImGuiCol_CheckMark,       ImVec4(1.00f, 1.00f, 1.00f, 1.0f));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                bool changed = Checkbox(label, val);
                PopStyleVar();
                PopStyleColor(4);
                Dummy(ImVec2(0, 6));
                return changed;
            };

            // ── Draw section ─────────────────────────────────────────────────
            DrawSectionHdr0("Draw");
            need_save |= StyledCheckbox(O("Draw Lines"),
                &persistent_bool[O("bESP_DrawPredictionLine")]);
            need_save |= StyledCheckbox(O("Draw Pockets"),
                &persistent_bool[O("bESP_DrawPocketsShotState")]);
            need_save |= StyledCheckbox(O("Show Enemy Lines"),
                &persistent_bool[O("bESP_EnemyLine")]);
            {
                bool changed = StyledCheckbox(O("Enable Play Button"),
                    &persistent_bool[O("bAutoAim")]);
                if (changed) {
                    AutoAim::bActive = persistent_bool[O("bAutoAim")];
                    need_save = true;
                }
            }
            need_save |= StyledCheckbox(O("Show Pockets"),
                &persistent_bool[O("bShowPockets")]);

            // ── Settings section ─────────────────────────────────────────────
            DrawSectionHdr0("Settings");

            {
                if (persistent_int[O("iLineThickness")] < 1) persistent_int[O("iLineThickness")] = 4;
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab,       ImVec4(0.12f, 0.45f, 0.95f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.20f, 0.55f, 1.00f, 1.0f));
                float avail = GetContentRegionAvail().x;
                // Label right-aligned
                const char* lbl1 = "Line Thickness";
                ImVec2 lsz1 = CalcTextSize(lbl1);
                TextColored(ImVec4(0.55f, 0.65f, 0.80f, 1.0f), "%s", lbl1);
                SameLine(avail - lsz1.x + 4.0f);
                SetNextItemWidth(avail * 0.52f);
                need_save |= SliderInt(O("##lineThick"), &persistent_int[O("iLineThickness")], 1, 10, "%d");
                Dummy(ImVec2(0, 6));
                int& menuSz = persistent_int[O("iMenuSizeOffset")];
                const char* lbl2 = "Fix Menu Size";
                TextColored(ImVec4(0.55f, 0.65f, 0.80f, 1.0f), "%s", lbl2);
                ImVec2 lsz2 = CalcTextSize(lbl2);
                SameLine(avail - lsz2.x + 4.0f);
                SetNextItemWidth(avail * 0.52f);
                bool changed = SliderInt(O("##menuSize"), &menuSz, -10, 10,
                    menuSz == 0 ? O("Normal") : "%d");
                need_save |= changed;
                PopStyleColor(3);
                PopStyleVar(2);
            }

            Dummy(ImVec2(0, 18));
            {
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.38f, 0.85f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.48f, 1.00f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f, 0.30f, 0.70f, 1.0f));
                if (Button(O("Save Config"), ImVec2(GetContentRegionAvail().x, 48.0f))) {
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

            ModeSwitch8(O("Aim Break"),       AimMode::EIGHTBALL_BREAK);
            ModeSwitch8(O("Aim Predict"),     AimMode::EIGHTBALL_PREDICT);
            ModeSwitch8(O("Aim Lock 8"),      AimMode::EIGHTBALL_8LOCK);

            // ══════════════════════════════════════════════════════════════
            // INJECT GHOST MODE — Unified Speed Exploit System
            // ══════════════════════════════════════════════════════════════
            {
                Dummy(ImVec2(0, 16));

                float gmW  = GetContentRegionAvail().x;
                ImDrawList* gmDl = GetWindowDrawList();

                // ── Animated ghost header ─────────────────────────────────
                float gmHdrH = 38.0f;
                ImVec2 gmHdrPos = GetCursorScreenPos();
                float tt = (float)GetTime();

                // Header BG — deep purple/ghost gradient
                gmDl->AddRectFilledMultiColor(
                    gmHdrPos, ImVec2(gmHdrPos.x + gmW, gmHdrPos.y + gmHdrH),
                    IM_COL32(20,  4, 38, 245), IM_COL32(32,  6, 55, 245),
                    IM_COL32(28,  5, 48, 245), IM_COL32(18,  3, 32, 245));
                // Animated purple pulse border
                {
                    float pa  = 0.35f + 0.28f * sinf(tt * 4.0f);
                    float pa2 = 0.20f + 0.15f * sinf(tt * 2.5f + 1.0f);
                    gmDl->AddRect(gmHdrPos,
                        ImVec2(gmHdrPos.x + gmW, gmHdrPos.y + gmHdrH),
                        IM_COL32(160, 40, 255, (int)(pa * 255)),
                        10.0f, ImDrawFlags_RoundCornersTop, 1.8f);
                    gmDl->AddRect(
                        ImVec2(gmHdrPos.x - 2, gmHdrPos.y - 2),
                        ImVec2(gmHdrPos.x + gmW + 2, gmHdrPos.y + gmHdrH + 2),
                        IM_COL32(200, 80, 255, (int)(pa2 * 255)),
                        12.0f, ImDrawFlags_RoundCornersTop, 3.0f);
                }
                // Ghost icon "G" + title
                SetWindowFontScale(0.95f);
                const char* gmTitle = "  ☠  INJECT GHOST MODE";
                ImVec2 gmTitleSz = CalcTextSize(gmTitle);
                SetCursorPosY(GetCursorPosY() + (gmHdrH - gmTitleSz.y) * 0.5f);
                SetCursorPosX(GetCursorPosX() + 10.0f);
                // Glow text effect
                ImVec2 gmTitleScreenPos = GetCursorScreenPos();
                float glowAlpha = 0.18f + 0.14f * sinf(tt * 3.8f);
                gmDl->AddText(
                    ImVec2(gmTitleScreenPos.x + 1, gmTitleScreenPos.y + 1),
                    IM_COL32(200, 80, 255, (int)(glowAlpha * 255 * 3)), gmTitle);
                TextColored(ImVec4(0.88f, 0.42f, 1.0f, 1.0f), "%s", gmTitle);
                SetWindowFontScale(1.0f);

                // ── Body card ─────────────────────────────────────────────
                Dummy(ImVec2(0, 2));
                ImVec2 gmBodyStart = GetCursorScreenPos();
                gmDl->ChannelsSplit(2);
                gmDl->ChannelsSetCurrent(1);

                Dummy(ImVec2(0, 10));

                // ── Toggle Ghost Mode ─────────────────────────────────────
                SetCursorPosX(GetCursorPosX() + 10.0f);
                {
                    bool wasActive = GhostMode::bActive;
                    if (ToggleSwitch("Inject Ghost Mode (ALL sabotage)", &GhostMode::bActive)) {
                        GhostMode::SetActive(GhostMode::bActive);
                    }
                    // Jika state berubah lewat toggle internal
                    if (GhostMode::bActive != wasActive) {
                        GhostMode::SetActive(GhostMode::bActive);
                    }
                }

                if (GhostMode::bActive) {
                    // ── Status badge aktif ────────────────────────────────
                    Dummy(ImVec2(0, 6));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    {
                        float bPulse = 0.70f + 0.30f * sinf(tt * 6.0f);
                        ImVec2 badgePos = GetCursorScreenPos();
                        float bW = gmW - 20.0f;
                        gmDl->AddRectFilled(badgePos,
                            ImVec2(badgePos.x + bW, badgePos.y + 22.0f),
                            IM_COL32(60, 10, 100, (int)(bPulse * 200)), 6.0f);
                        gmDl->AddRect(badgePos,
                            ImVec2(badgePos.x + bW, badgePos.y + 22.0f),
                            IM_COL32(200, 80, 255, (int)(bPulse * 230)), 6.0f, 0, 1.2f);
                        const char* statusTxt = "  ◆ GHOST AKTIF — SEMUA INJECT BERJALAN";
                        ImVec2 stSz = CalcTextSize(statusTxt);
                        gmDl->AddText(
                            ImVec2(badgePos.x + (bW - stSz.x) * 0.5f, badgePos.y + (22.0f - stSz.y) * 0.5f),
                            IM_COL32(230, 150, 255, 255), statusTxt);
                        Dummy(ImVec2(0, 28));
                    }

                    // ── Speed Exploit ─────────────────────────────────────
                    Dummy(ImVec2(0, 4));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    TextColored(ImVec4(0.78f, 0.38f, 1.0f, 1.0f), "Speed Exploit:");

                    Dummy(ImVec2(0, 4));

                    // Speed category badges (Min / Normal / High / Extreme / ULTRA MAX)
                    {
                        struct { const char* lbl; float val; ImVec4 col; } speeds[] = {
                            { "MIN",       1.0f,  ImVec4(0.40f, 0.40f, 0.55f, 1.0f) },
                            { "NORMAL",   10.0f,  ImVec4(0.20f, 0.60f, 0.90f, 1.0f) },
                            { "HIGH",     35.0f,  ImVec4(0.90f, 0.60f, 0.10f, 1.0f) },
                            { "EXTREME",  70.0f,  ImVec4(0.95f, 0.25f, 0.25f, 1.0f) },
                            { "ULTRA MAX",100.0f, ImVec4(0.88f, 0.20f, 1.00f, 1.0f) },
                        };
                        float btnW = (gmW - 20.0f - 4.0f * 4.0f) / 5.0f;
                        SetCursorPosX(GetCursorPosX() + 10.0f);
                        for (int si = 0; si < 5; si++) {
                            bool isCurSpeed = (GhostMode::fSpeed >= speeds[si].val - 0.5f &&
                                (si == 4 || GhostMode::fSpeed < speeds[si+1 < 5 ? si+1 : 4].val - 0.5f));
                            ImVec4 bc = isCurSpeed
                                ? ImVec4(speeds[si].col.x * 0.6f + 0.05f,
                                         speeds[si].col.y * 0.5f + 0.05f,
                                         speeds[si].col.z * 0.6f + 0.10f, 1.0f)
                                : ImVec4(0.08f, 0.06f, 0.14f, 1.0f);
                            ImVec4 bh = ImVec4(bc.x + 0.08f, bc.y + 0.06f, bc.z + 0.10f, 1.0f);
                            PushID(si + 2000);
                            PushStyleColor(ImGuiCol_Button,        bc);
                            PushStyleColor(ImGuiCol_ButtonHovered, bh);
                            PushStyleColor(ImGuiCol_ButtonActive,  bc);
                            PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                            if (isCurSpeed) {
                                // Glowing border for selected
                                ImVec2 btnPos = GetCursorScreenPos();
                                gmDl->AddRect(btnPos,
                                    ImVec2(btnPos.x + btnW, btnPos.y + 28.0f),
                                    ImColor(speeds[si].col), 6.0f, 0, 1.5f);
                            }
                            if (Button(speeds[si].lbl, ImVec2(btnW, 28.0f)))
                                GhostMode::fSpeed = speeds[si].val;
                            PopStyleVar();
                            PopStyleColor(3);
                            PopID();
                            if (si < 4) SameLine(0, 4.0f);
                        }
                    }

                    // ── Speed fine-tune slider ────────────────────────────
                    Dummy(ImVec2(0, 8));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    TextColored(ImVec4(0.62f, 0.62f, 0.72f, 1.0f), "Fine Tune:");
                    Dummy(ImVec2(0, 4));
                    {
                        // Warna slider berubah sesuai kecepatan (biru→merah→ungu)
                        float spd = GhostMode::fSpeed;
                        float r   = ImClamp(spd / 100.0f, 0.0f, 1.0f);
                        ImVec4 grabCol  = ImVec4(0.12f + r * 0.76f, 0.55f - r * 0.35f, 1.0f - r * 0.5f, 1.0f);
                        ImVec4 grabActv = ImVec4(grabCol.x + 0.1f, grabCol.y + 0.08f, grabCol.z + 0.05f, 1.0f);

                        PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                        PushStyleVar(ImGuiStyleVar_GrabRounding,  10.0f);
                        PushStyleColor(ImGuiCol_FrameBg,          ImVec4(0.08f, 0.04f, 0.14f, 1.0f));
                        PushStyleColor(ImGuiCol_SliderGrab,       grabCol);
                        PushStyleColor(ImGuiCol_SliderGrabActive, grabActv);
                        SetNextItemWidth(gmW - 20.0f);
                        SliderFloat("##ghostSpeed", &GhostMode::fSpeed, 1.0f, 100.0f, "%.1f");
                        PopStyleColor(3);
                        PopStyleVar(2);
                    }

                    // ── Speed info label ──────────────────────────────────
                    Dummy(ImVec2(0, 6));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    {
                        const char* spLbl = GhostMode::GetSpeedLabel(GhostMode::fSpeed);
                        bool isUltra = (GhostMode::fSpeed >= 85.0f);
                        bool isHigh  = (GhostMode::fSpeed >= 50.0f);
                        ImVec4 spColor = isUltra
                            ? ImVec4(0.90f, 0.20f, 1.00f, 1.0f)
                            : isHigh
                                ? ImVec4(1.00f, 0.28f, 0.28f, 1.0f)
                                : ImVec4(0.55f, 0.80f, 1.00f, 1.0f);
                        if (isUltra) {
                            // Flashing warning untuk ultra max
                            float flashA = 0.75f + 0.25f * sinf(tt * 8.0f);
                            spColor.w = flashA;
                        }
                        TextColored(spColor, "%s", spLbl);
                    }

                    // ── Ctrl Turn pocket selector ─────────────────────────
                    Dummy(ImVec2(0, 10));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    TextColored(ImVec4(0.62f, 0.38f, 0.88f, 1.0f), "Arahkan aim musuh ke pocket:");
                    Dummy(ImVec2(0, 4));
                    {
                        const char* pocketNames[] = {
                            "0 - Top Left", "1 - Top Center", "2 - Top Right",
                            "3 - Bot Right", "4 - Bot Center", "5 - Bot Left"
                        };
                        PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                        PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.10f, 0.04f, 0.18f, 1.0f));
                        PushStyleColor(ImGuiCol_Header,         ImVec4(0.35f, 0.08f, 0.60f, 1.0f));
                        PushStyleColor(ImGuiCol_HeaderHovered,  ImVec4(0.45f, 0.12f, 0.75f, 1.0f));
                        SetNextItemWidth(gmW - 20.0f);
                        Combo("##ghostCtrlPocket", &GhostMode::iCtrlPocket, pocketNames, 6);
                        PopStyleColor(3);
                        PopStyleVar();
                    }

                    // ── WARNING badge ─────────────────────────────────────
                    if (GhostMode::fSpeed >= 85.0f) {
                        Dummy(ImVec2(0, 8));
                        SetCursorPosX(GetCursorPosX() + 10.0f);
                        float wPulse = 0.60f + 0.40f * sinf(tt * 9.0f);
                        TextColored(
                            ImVec4(1.0f, 0.12f, 0.12f, wPulse),
                            "!!! ULTRA MAX — Musuh tidak bisa gerak !!!");
                    } else if (GhostMode::fSpeed >= 35.0f) {
                        Dummy(ImVec2(0, 6));
                        SetCursorPosX(GetCursorPosX() + 10.0f);
                        TextColored(ImVec4(1.0f, 0.55f, 0.10f, 1.0f),
                            "!  Kecepatan Tinggi — efek kuat");
                    }
                } else {
                    // Inactive hint
                    Dummy(ImVec2(0, 8));
                    SetCursorPosX(GetCursorPosX() + 10.0f);
                    TextColored(ImVec4(0.38f, 0.34f, 0.46f, 1.0f),
                        "Aktifkan untuk inject semua sabotase\nke musuh secara bersamaan.");
                    Dummy(ImVec2(0, 4));
                }

                Dummy(ImVec2(0, 12));

                // Draw body BG retroactively
                float gmBodyActualH = GetCursorScreenPos().y - gmBodyStart.y;
                gmDl->ChannelsSetCurrent(0);
                gmDl->AddRectFilledMultiColor(
                    gmBodyStart,
                    ImVec2(gmBodyStart.x + gmW, gmBodyStart.y + gmBodyActualH),
                    IM_COL32(14,  5, 26, 228), IM_COL32(18,  6, 32, 228),
                    IM_COL32(12,  4, 22, 228), IM_COL32(16,  5, 28, 228));
                gmDl->AddRect(gmBodyStart,
                    ImVec2(gmBodyStart.x + gmW, gmBodyStart.y + gmBodyActualH),
                    IM_COL32(120, 30, 200, 110), 10.0f, ImDrawFlags_RoundCornersBottom, 1.0f);
                gmDl->ChannelsMerge();
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

            break;
        }

        case 4:
        default:
            break;

    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleColor();
}


INLINE void DrawMenu(ImGuiIO& io) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        // Restore state dari persistent storage saat pertama kali jalan
        {
            static bool s_stateRestored = false;
            if (!s_stateRestored) {
                AutoAim::bActive = persistent_bool[O("bAutoAim")];
                s_stateRestored  = true;
            }
        }

        buttonClicker.Update();
        AutoAim::Update();
        PocketSelector::Update();
        GhostMode::Update();

        g_espStateReady = false;
        g_espIsInGame   = false;    // reset tiap frame — DrawESP akan set true jika memang in-game

        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        // ── Gate: semua UI hanya aktif saat sedang di dalam match ─────────
        static bool s_wasInGame = false;
        if (!g_espIsInGame) {
            // Keluar dari match → force tutup semua UI + reset tracker
            g_menu.isOpen    = false;
            g_menu.menuAlpha = 0.0f;
            s_wasInGame      = false;
            return;
        }

        // Auto-open menu setiap kali pertama masuk match (transisi out-of-game → in-game)
        if (!s_wasInGame) {
            g_menu.isOpen = true;
        }
        s_wasInGame = true;

        if (g_espStateReady && persistent_bool[O("bAutoAim")]) {
            DrawToggleButton();
        }

        // ── Full menu ─────────────────────────────────────────────────────
        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarWidth * sizeScale;
            float winH = 700.0f * sizeScale;
            
            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            
            // ── Smooth Dark Glass window style ────────────────────────────
            float sidebarW = 118.0f * sizeScale;

            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.03f, 0.07f, 0.93f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 26.0f);
            PushStyleVar(ImGuiStyleVar_ChildRounding,  18.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);

            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

            if (Begin(O("##MainMenu"), &g_menu.isOpen, winFlags)) {
                // Left vertical sidebar, then content to the right
                DrawSidebar(sidebarW, winH);
                SameLine(0, 0);
                DrawContentArea(winW - sidebarW, winH);

                // ── Glass border overlay — Smooth Dark Glass / Ice Blue ────
                ImDrawList* bdl = GetWindowDrawList();
                ImVec2 bPos  = GetWindowPos();
                ImVec2 bSize = GetWindowSize();
                ImVec2 bMax  = ImVec2(bPos.x + bSize.x, bPos.y + bSize.y);
                float  br    = 26.0f;

                // Layer 1 — diffuse outer glow (ice blue)
                bdl->AddRect(
                    ImVec2(bPos.x - 3.0f, bPos.y - 3.0f),
                    ImVec2(bMax.x + 3.0f, bMax.y + 3.0f),
                    IM_COL32(0, 165, 210, 18), br + 5.0f, 0, 6.0f);

                // Layer 2 — medium ice glow
                bdl->AddRect(
                    ImVec2(bPos.x - 1.0f, bPos.y - 1.0f),
                    ImVec2(bMax.x + 1.0f, bMax.y + 1.0f),
                    IM_COL32(0, 180, 225, 38), br + 2.0f, 0, 2.5f);

                // Layer 3 — main crisp dark-glass edge
                bdl->AddRect(bPos, bMax, IM_COL32(0, 160, 210, 65), br, 0, 1.0f);

                // Layer 4 — inner pale rim
                bdl->AddRect(
                    ImVec2(bPos.x + 1.0f, bPos.y + 1.0f),
                    ImVec2(bMax.x - 1.0f, bMax.y - 1.0f),
                    IM_COL32(180, 235, 255, 14), br - 1.0f, 0, 0.7f);

                // Top gloss bar — white horizontal sheen
                bdl->AddRectFilledMultiColor(
                    ImVec2(bPos.x + 26.0f, bPos.y + 1.0f),
                    ImVec2(bMax.x - 26.0f, bPos.y + 2.2f),
                    IM_COL32(255,255,255, 0), IM_COL32(255,255,255,50),
                    IM_COL32(255,255,255,50), IM_COL32(255,255,255, 0));

                // Bottom dark rim
                bdl->AddRectFilledMultiColor(
                    ImVec2(bPos.x + 26.0f, bMax.y - 2.0f),
                    ImVec2(bMax.x - 26.0f, bMax.y - 0.5f),
                    IM_COL32(0,0,0, 0), IM_COL32(0,0,0,55),
                    IM_COL32(0,0,0,55), IM_COL32(0,0,0, 0));
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
        // Floating button hanya tampil saat sedang di dalam match (g_espIsInGame dari frame sebelumnya)
        if (g_espIsInGame) DrawFloatingButton(io);
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
        float fs = GImGui->FontSize * 1.22f;
        ImVec2 ts2 = GImGui->Font->CalcTextSizeA(fs, FLT_MAX, 0.f, lbl);
        // Shadow only (no background box)
        fgDl->AddText(GImGui->Font, fs,
            ImVec2(cp.x + 1.5f, cp.y + 1.5f),
            IM_COL32(0, 60, 20, 120), lbl);
        // Main text — bright green, no background
        fgDl->AddText(GImGui->Font, fs, cp,
            IM_COL32(30, 255, 90, 230), lbl);
        ImGui::Dummy(ImVec2(ts2.x, fs));
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
