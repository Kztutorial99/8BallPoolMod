#pragma once

namespace AutoQueue {
    void TryStartMatch() {
        if (buttonClicker.Active) return;
        // Click the PLAY / Find Match button — approximately center-bottom of lobby screen
        buttonClicker.Click(ImVec2(Width * 0.5f, Height * 0.80f), 0.25f);
        g_aqCounting = false;
    }
}
