#pragma once

#include <thread>
#include <chrono>
#include <atomic>

// ── Inject Tap Pocket ──────────────────────────────────────────────────────────
// Setelah aim selesai, inject tap nyata ke posisi pocket di layar
// untuk trigger pocket nomination UI game (bukan hanya tulis memory).
//
// Pocket index (standar 8 Ball Pool Miniclip):
//   0 = Top-Left    1 = Top-Center    2 = Top-Right
//   3 = Bottom-Right 4 = Bottom-Center 5 = Bottom-Left
//
// Koordinat world: X ∈ [-127, 127], Y ∈ [-63.5, 63.5]
//   Y positif = atas meja di layar, Y negatif = bawah meja di layar
// ─────────────────────────────────────────────────────────────────────────────

namespace InjectTapPocket {
    bool bEnabled = false;

    static std::atomic<bool> s_tapPending{false};

    // Posisi pocket dalam world space — sesuai urutan index game
    static const Vec2d POCKET_WORLD[TABLE_POCKETS_COUNT] = {
        Vec2d(-TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT),   // 0: Top-Left
        Vec2d( 0.0,               TABLE_HALF_HEIGHT),   // 1: Top-Center
        Vec2d( TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT),   // 2: Top-Right
        Vec2d( TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT),   // 3: Bottom-Right
        Vec2d( 0.0,              -TABLE_HALF_HEIGHT),   // 4: Bottom-Center
        Vec2d(-TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT),   // 5: Bottom-Left
    };

    // Konversi pocket index → screen position
    static ImVec2 GetPocketScreenPos(int pocketIdx) {
        if (pocketIdx < 0 || pocketIdx >= TABLE_POCKETS_COUNT)
            return ImVec2(-1.0f, -1.0f);

        // Coba baca dari data game dulu (lebih akurat)
        if (sharedGameManager) {
            auto table = sharedGameManager.mTable();
            if (table) {
                auto tableProps = table.mTableProperties();
                if (tableProps) {
                    Vec2d* pockets = tableProps.mPockets();
                    if (pockets) {
                        Vec2d wp = pockets[pocketIdx];
                        // Validasi nilai masuk akal
                        if (wp.x >= -TABLE_HALF_WIDTH - 20.0 &&
                            wp.x <=  TABLE_HALF_WIDTH + 20.0 &&
                            wp.y >= -TABLE_HALF_HEIGHT - 20.0 &&
                            wp.y <=  TABLE_HALF_HEIGHT + 20.0) {
                            return WorldToScreen(wp);
                        }
                    }
                }
            }
        }

        // Fallback: gunakan posisi hardcoded
        return WorldToScreen(POCKET_WORLD[pocketIdx]);
    }

    // Inject tap nyata ke posisi pocket di layar
    // delayMs: waktu tunggu sebelum tap (beri waktu game munculkan UI nomination)
    static void TapPocket(int pocketIdx, int delayMs = 250) {
        if (!bEnabled) return;
        if (pocketIdx < 0 || pocketIdx >= TABLE_POCKETS_COUNT) return;
        if (s_tapPending.load()) return;  // hindari double-tap

        ImVec2 pos = GetPocketScreenPos(pocketIdx);
        if (pos.x < 0.0f || pos.y < 0.0f) return;

        float tx = pos.x;
        float ty = pos.y;

        s_tapPending = true;
        std::thread([tx, ty, delayMs]() {
            // Tunggu game tampilkan UI nomination
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Inject tap: BEGIN → sedikit tahan → END
            NativeTouchesBegin(0, tx, ty);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            NativeTouchesEnd(0, tx, ty);

            s_tapPending = false;
        }).detach();
    }
}
