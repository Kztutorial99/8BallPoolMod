#pragma once

#include "game/Types.h"
#include "game/GameManager.h"
#include "game/FrictionProperties.h"
#include "imgui/inc/persistence.h"

// ── PhysicsHack ──────────────────────────────────────────────────────────────
// Modifies live game physics constants that are also read by the prediction
// engine — so prediction stays accurate even when physics are changed.
//
// Features:
//   1. Max Power Modifier  — CUE_PROPERTIES_MAX_POWER (libmain+0x4e49410)
//   2. Max Spin Multiplier — CUE_PROPERTIES_SPIN      (libmain+0x4e49418)
//   3. Table Friction Hack — table._frictionProperties()
//
// Call PhysicsHack::Apply() once per frame from DrawESP.
// ─────────────────────────────────────────────────────────────────────────────

namespace PhysicsHack {

    // Default physics constants (game's original values)
    static constexpr double DEFAULT_MAX_POWER            = 666.0;
    static constexpr double DEFAULT_MAX_SPIN             = 1.0;
    static constexpr double DEFAULT_ROLLING_FRICTION     = 0.0111;
    static constexpr double DEFAULT_SLIDING_FRICTION     = 0.20;
    static constexpr double DEFAULT_VEL_REDUCTION_ROLLING = 10.878;
    static constexpr double DEFAULT_DELTA_SPIN           = 9.8;

    static bool g_vipPatched = false;

    // ARM64: MOVZ W0, #1 then RET — patches a bool-returning function to always return true
    static constexpr uint8_t PATCH_RETURN_TRUE[8] = {
        0x20, 0x00, 0x80, 0x52,   // MOVZ W0, #1
        0xC0, 0x03, 0x5F, 0xD6    // RET
    };

    static void Apply() {
        // ── 1. Max Power Modifier ──────────────────────────────────────────
        if (persistent_bool["bPhysicsHack"]) {
            double targetPower = (double)persistent_float["fMaxPower"];
            if (targetPower < 100.0) targetPower = 100.0;
            if (targetPower > 5000.0) targetPower = 5000.0;
            CUE_PROPERTIES_MAX_POWER = targetPower;

            // ── 2. Max Spin Multiplier ─────────────────────────────────────
            double targetSpin = (double)persistent_float["fMaxSpin"];
            if (targetSpin < 0.1) targetSpin = 0.1;
            if (targetSpin > 5.0) targetSpin = 5.0;
            CUE_PROPERTIES_SPIN = targetSpin;
        } else {
            // Restore defaults when disabled
            CUE_PROPERTIES_MAX_POWER = DEFAULT_MAX_POWER;
            CUE_PROPERTIES_SPIN      = DEFAULT_MAX_SPIN;
        }

        // ── 3. Table Friction Hack ─────────────────────────────────────────
        // Lower friction = balls roll farther (fast table)
        // Higher friction = balls stop sooner (slow table)
        if (persistent_bool["bTableSpeedHack"] && sharedGameManager) {
            Table table = sharedGameManager.mTable;
            if (table) {
                FrictionProperties& fp = table._frictionProperties();
                double mul = (double)persistent_float["fTableFriction"];
                if (mul < 0.05) mul = 0.05;
                if (mul > 5.0)  mul = 5.0;

                fp._coefficientOfRollingFriction   = DEFAULT_ROLLING_FRICTION      * mul;
                fp._coefficientOfSlidingFriction   = DEFAULT_SLIDING_FRICTION      * mul;
                fp._velocityReductionRollingFactor = DEFAULT_VEL_REDUCTION_ROLLING / mul;
                fp._deltaSpinFactor                = DEFAULT_DELTA_SPIN            * mul;
            }
        }
    }

    static void PatchVIP() {
        if (g_vipPatched) return;
        if (!libmain) return;

        // Patch isVIPFeatureActive (libmain + 0x368b390) → always return true
        EditMemory(libmain + 0x368b390, 8, PATCH_RETURN_TRUE);
        // Patch isPayingUser   (libmain + 0x358fdc4) → always return true
        EditMemory(libmain + 0x358fdc4, 8, PATCH_RETURN_TRUE);

        g_vipPatched = true;
        LOGI("PhysicsHack: VIP patched");
    }

    static void UnpatchVIP() {
        g_vipPatched = false;
    }

} // namespace PhysicsHack
