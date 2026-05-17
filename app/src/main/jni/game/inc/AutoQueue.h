#pragma once

namespace AutoQueue {

    // Entry fees matching the 17 table buttons in menu (same order)
    static const int64_t TABLE_ENTRY_FEES[17] = {
        100LL,          // M1  London   100
        200LL,          // M2  Moscow   200
        1000LL,         // M3  Kyiv     1k
        2500LL,         // M4  Amst.    2.5k
        10000LL,        // M5  Istan.   10k
        50000LL,        // M6  Monaco   50k
        100000LL,       // M7  Berlin   100k
        500000LL,       // M8  Sydney   500k
        1000000LL,      // M9  London   1M
        2000000LL,      // M10 Dubai    2M
        5000000LL,      // M11 Sing.    5M
        8000000LL,      // M12 Cairo    8M
        10000000LL,     // M13 Bangkok  10M
        20000000LL,     // M14 Vienna   20M
        30000000LL,     // M15 SF       30M
        50000000LL,     // M16 Shanghai 50M
        200000000LL,    // M17 London   200M
    };

    static const char* TABLE_LABELS[17] = {
        "100", "200", "1k", "2.5k", "10k", "50k", "100k",
        "500k", "1M", "2M", "5M", "8M", "10M", "20M", "30M", "50M", "200M"
    };

    // Parse coin string "1,234,567" or "1.234.567" -> int64
    static int64_t ParseCoins(const std::string& s) {
        int64_t result = 0;
        for (char c : s) {
            if (c >= '0' && c <= '9')
                result = result * 10 + (c - '0');
        }
        return result;
    }

    // Read current coin balance from game memory
    static int64_t GetCoins() {
        if (!sharedUserInfo) return 0;
        std::string coinStr = ReadNSString(sharedUserInfo.coins());
        return ParseCoins(coinStr);
    }

    // Returns best table index for given coin balance and bet percentage (1-100)
    static int PickSmartTable(int64_t coins, int betPercent) {
        if (betPercent < 1)   betPercent = 1;
        if (betPercent > 100) betPercent = 100;
        int64_t targetBet = coins * betPercent / 100;

        int bestIdx = 0;
        for (int i = 16; i >= 0; i--) {
            if (TABLE_ENTRY_FEES[i] <= targetBet) {
                bestIdx = i;
                break;
            }
        }
        return bestIdx;
    }

    // Returns the smart-calculated table index, or -1 if not in Smart mode
    static int GetSmartTableIndex() {
        if (persistent_int["iAutoQueue_Mode"] != 1) return -1;
        int64_t coins = GetCoins();
        if (coins <= 0) return 0;
        int betPercent = persistent_int["iAutoQueue_BetPercent"];
        if (betPercent < 1) betPercent = 20;
        return PickSmartTable(coins, betPercent);
    }

    // Returns formatted coin string with K/M suffix
    static std::string FormatCoins(int64_t coins) {
        char buf[32];
        if (coins >= 1000000000LL)
            snprintf(buf, sizeof(buf), "%.2fB", coins / 1000000000.0);
        else if (coins >= 1000000LL)
            snprintf(buf, sizeof(buf), "%.2fM", coins / 1000000.0);
        else if (coins >= 1000LL)
            snprintf(buf, sizeof(buf), "%.1fK", coins / 1000.0);
        else
            snprintf(buf, sizeof(buf), "%lld", (long long)coins);
        return std::string(buf);
    }

    void TryStartMatch() {
        if (buttonClicker.Active) return;

        // Smart mode: auto-calculate the best table before queuing
        if (persistent_int["iAutoQueue_Mode"] == 1) {
            int smartIdx = GetSmartTableIndex();
            if (smartIdx >= 0) {
                persistent_int["iAutoQueue_FixTable"] = smartIdx;
                save_persistence();
            }
        }

        buttonClicker.Click(ImVec2(Width * 0.5f, Height * 0.80f), 0.25f);
        g_aqCounting = false;
    }
}
