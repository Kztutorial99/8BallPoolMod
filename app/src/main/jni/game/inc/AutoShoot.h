#pragma once

// ── Auto Shoot ─────────────────────────────────────────────────────────────
// Mensimulasikan swipe power gauge secara otomatis setelah aim selesai.
// Tidak perlu klik/swipe manual sama sekali — mod menembak sendiri.
//
// Alur kerja:
//   1. AutoAim selesai (bAimed == true)
//   2. AutoShoot menunggu AIM_TO_SHOOT_DELAY detik (agar angle ter-set di game)
//   3. Hitung posisi power gauge dari mPowerGaugeLocation (kiri/kanan)
//   4. PowerSlider simulasi swipe: press bawah → drag ke atas → release → SHOT
//   5. Cooldown COOLDOWN_AFTER_SHOT detik sebelum bisa tembak lagi
// ─────────────────────────────────────────────────────────────────────────

#include "../../mod/PowerSlider.h"
#include "../UserSettingsManager.h"

// ── Definisi extern yang dibutuhkan PowerSlider.h ────────────────────────

Candidate  g_CurrentCandidate  = { -1, 0.0, 0.0, -1, 0.0 };
Point2D    lastFailedCuePos    = { -1000.0, -1000.0 };

// Validasi tembakan: hanya boleh saat giliran kita (state 4) dan game aktif
bool IsShotValid() {
    if (!sharedGameManager) return false;
    GameStateManager gsm = sharedGameManager.mStateManager;
    if (!gsm) return false;
    return gsm.getCurrentStateId() == 4;
}

// ── Namespace AutoShoot ──────────────────────────────────────────────────

namespace AutoShoot {

    bool bEnabled = false;

    // Delay (detik) antara aim selesai → tembak,
    // agar game sempat update angle/power sebelum swipe dilakukan
    static constexpr float AIM_TO_SHOOT_DELAY   = 0.45f;

    // Cooldown (detik) setelah tembakan untuk mencegah double-shoot
    static constexpr float COOLDOWN_AFTER_SHOT  = 2.8f;

    static float s_aimDelay      = 0.f;
    static float s_cooldown      = 0.f;
    static bool  s_waitingToShoot = false;
    static int   s_lastStateId   = -1;

    // ── Hitung rect power gauge dari pengaturan user + ukuran layar ──────
    // ImVec4(x_kiri, y_bawah, lebar, jarak_drag)
    // jarak_drag negatif = seret ke atas (lebih besar power)
    static ImVec4 GetPowerGaugeRect() {
        int location = 0;
        if (sharedUserSettingsManager) {
            location = sharedUserSettingsManager.mPowerGaugeLocation();
        }

        float gaugeW  = Width  * 0.075f;
        float startY  = Height * 0.780f;   // titik tekan (bawah gauge)
        float dragH   = -(Height * 0.285f); // seret ke atas (negatif)

        float gaugeX;
        if (location == 1) {
            gaugeX = Width * 0.888f;       // kanan
        } else {
            gaugeX = Width * 0.037f;       // kiri
        }

        return ImVec4(gaugeX, startY, gaugeW, dragH);
    }

    // Reset state (dipanggil saat giliran berakhir)
    void Reset() {
        s_waitingToShoot = false;
        s_aimDelay       = 0.f;
    }

    // Dipanggil setiap frame dari GL render thread (di DrawMenu)
    void Update() {
        if (!bEnabled) return;

        float dt = ImGui::GetIO().DeltaTime;

        // Cooldown pasca-tembakan
        if (s_cooldown > 0.f) {
            s_cooldown -= dt;
            return;
        }

        // Jika PowerSlider sedang aktif, biarkan selesai dulu
        if (powerSlider.Active) return;

        if (!sharedGameManager) return;
        GameStateManager gsm = sharedGameManager.mStateManager;
        if (!gsm) return;
        int stateId = gsm.getCurrentStateId();

        // Reset saat giliran berakhir (state 4 → bukan 4)
        if (s_lastStateId == 4 && stateId != 4) {
            Reset();
        }
        s_lastStateId = stateId;

        // Hanya aktif saat giliran kita
        if (stateId != 4) return;

        // Tunggu aim selesai dulu
        if (!AutoAim::bAimed.load()) {
            s_waitingToShoot = false;
            s_aimDelay       = 0.f;
            return;
        }

        // Aim baru selesai — mulai hitung mundur delay
        if (!s_waitingToShoot) {
            s_waitingToShoot = true;
            s_aimDelay       = 0.f;
        }

        s_aimDelay += dt;
        if (s_aimDelay < AIM_TO_SHOOT_DELAY) return;

        // ── TEMBAK ────────────────────────────────────────────────────────
        ImVec4 rect = GetPowerGaugeRect();

        // DragTime 0.7s ke max power, HoldTime 0.35s tahan sebelum release
        powerSlider.SimulateDrag(rect, 0.f, 0.7f, 0.35f);

        s_waitingToShoot = false;
        s_aimDelay       = 0.f;
        s_cooldown       = COOLDOWN_AFTER_SHOT;

        // Reset aim agar siklus bisa dimulai lagi di giliran berikutnya
        AutoAim::ResetAimed();
    }

} // namespace AutoShoot
