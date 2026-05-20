#pragma once

// ════════════════════════════════════════════════════════════════════════
// EXPERIMENT ENGINE v3 — FluxPro Research Lab (CRASH-SAFE BUILD)
// Semua physics-write sekarang ONE-SHOT, bukan per-frame.
// Tick() HANYA baca nilai + tulis UI-state yg aman.
// ════════════════════════════════════════════════════════════════════════

#include "../include/includes.h"
#include "../game/GameManager.h"
#include "../game/UserInfo.h"
#include "../game/CCDirector.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

namespace Experiment {

    // ─── State: [1] mAimOffset ───────────────────────────────────────────
    static bool  bAimOffsetActive = false;
    static float fAimOffsetX      = 0.0f;
    static float fAimOffsetY      = 0.0f;
    static double rdAimOffX  = 0.0, rdAimOffY = 0.0;
    static double rdEnglishX = 0.0, rdEnglishY = 0.0;

    // ─── State: [2] CUE_MAX_POWER ────────────────────────────────────────
    static bool   bMaxPowerActive = false;
    static float  fMaxPowerMul    = 2.0f;
    static double origMaxPower    = 0.0;
    static double rdMaxPowerNow   = 0.0;

    // ─── State: [3] CUE_SPIN ─────────────────────────────────────────────
    static bool   bMaxSpinActive  = false;
    static float  fMaxSpinMul     = 2.0f;
    static double origMaxSpin     = 0.0;
    static double rdMaxSpinNow    = 0.0;

    // ─── State: [4] VIP Hook ─────────────────────────────────────────────
    static bool bVIPActive = false;

    // ─── State: [5] Pocket Nomination Bypass ─────────────────────────────
    static bool bNomBypass = false;

    // ─── State: [6] Golden Shot Mode ─────────────────────────────────────
    static bool bGoldenShot = false;

    // ─── State: [7] Packet Logger ────────────────────────────────────────
    static bool bPacketLogActive     = false;
    static bool bPacketHookInstalled = false;
    static std::atomic<int> iSendCount{0};
    static std::atomic<int> iRecvCount{0};
    static std::mutex       mtxPkt;
    static char  szLastSend[160] = "—";
    static char  szLastRecv[160] = "—";
    static size_t lastSendLen = 0, lastRecvLen = 0;
    static ssize_t (*_origSend)(int, const void*, size_t, int) = nullptr;
    static ssize_t (*_origRecv)(int, void*, size_t, int)       = nullptr;

    // ─── State: [8] API Probe ────────────────────────────────────────────
    static std::atomic<bool> bAPITestRunning{false};
    static char szAPIResult[256] = "—";

    // ─── State: [9] Shot Power Force Max ─────────────────────────────────
    // SAFE: mPower adalah UI state, bukan physics state aktif
    static bool   bShotPowerMax = false;
    static double rdPowerNow    = 0.0;

    // ─── State: [10] Aim Angle Override ──────────────────────────────────
    // SAFE: mAimAngle adalah UI state saat aiming
    static bool  bAimAngleOverride = false;
    static float fAimAngleDeg      = 0.0f;
    static double rdCurrentAngle   = 0.0;

    // ─── State: [11] Enemy Ball Sink (ONE-SHOT) ───────────────────────────
    // CRASH FIX: bukan toggle per-frame — hanya one-shot button
    static int  iSinkCount    = 0;
    static char szSinkResult[64] = "—";

    // ─── State: [12] Zero Friction (write-on-toggle-change only) ──────────
    static bool   bZeroFriction   = false;
    static bool   bFrictionSaved  = false;
    static bool   bFrictionPrev   = false;  // detect toggle change
    static double origFriction[7] = {};

    // ─── State: [13] Cue Ball Ghost ───────────────────────────────────────
    // SAFE: radius hanya di-write saat toggle berubah, bukan setiap frame
    static bool   bCueBallGhost     = false;
    static bool   bCueBallGhostPrev = false;
    static float  fGhostRadius      = 0.25f;
    static double origCueBallRadius = 0.0;
    static double rdCueBallRadius   = 0.0;

    // ─── State: [14] Cue Ball Warp (ONE-SHOT) ────────────────────────────
    // CRASH FIX: write sekali saat tombol ditekan, tidak setiap frame
    static float fWarpX = 0.0f;
    static float fWarpY = 0.0f;

    // ─── State: [15] Velocity Burst (ONE-SHOT) ───────────────────────────
    static float fBurstStrength = 80.0f;

    // ─── State: [16] Ruleset Nomination Mode Override ─────────────────────
    // SAFE: hanya field 0x068 yg sudah verified (getPocketNominationMode)
    static bool bRuleNomZero = false;

    // ─── State: [17] UserInfo Coins Write (ONE-SHOT) ──────────────────────
    static float fCoinsValue  = 9999999.0f;
    static char  szCoinsResult[64] = "—";

    // ─── State: [18] BACON Server Probe ──────────────────────────────────
    static std::atomic<bool> bBACONRunning{false};
    static char szBACONResult[256] = "—";

    // ─── State: [19] S3 Config Probe ─────────────────────────────────────
    static std::atomic<bool> bS3Running{false};
    static char szS3Result[256] = "—";

    // ─── State: [20] CCDirector Memory Scan (READ-ONLY) ──────────────────
    // CRASH FIX: scan-only, tidak write ke memory CCDirector
    static bool bDirScan      = false;
    static int  iDirScanOffset= 0x100;
    static char szDirScan[384]= "—";

    // ════════════════════════════════════════════════════════════════════
    // UTILITY
    // ════════════════════════════════════════════════════════════════════

    static inline bool SafeAddr(uintptr_t a) {
        return a > 0x4000UL && a < 0x7FFFFF000000ULL;
    }

    static bool SafeWriteDouble(uintptr_t addr, double val) {
        if (!SafeAddr(addr)) return false;
        int pgsz = getpagesize();
        uintptr_t ps = addr & ~((uintptr_t)(pgsz - 1));
        if (mprotect((void*)ps, pgsz * 2, PROT_READ | PROT_WRITE) != 0) return false;
        *(double*)addr = val;
        mprotect((void*)ps, pgsz * 2, PROT_READ);
        return true;
    }

    static uintptr_t GetTableInst() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return 0;
        uintptr_t tbl = *(uintptr_t*)(sharedGameManager.instance + 0x3E8);
        return SafeAddr(tbl) ? tbl : 0;
    }

    static uintptr_t GetVCInst() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return 0;
        uintptr_t vc = *(uintptr_t*)(sharedGameManager.instance + 0x4B8);
        return SafeAddr(vc) ? vc : 0;
    }

    static uintptr_t GetVGInst() {
        uintptr_t vc = GetVCInst();
        if (!vc) return 0;
        uintptr_t vg = *(uintptr_t*)(vc + 0x3A8);
        return SafeAddr(vg) ? vg : 0;
    }

    static uintptr_t GetVECBase() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return 0;
        uintptr_t vec = *(uintptr_t*)(sharedGameManager.instance + 0x4C8);
        return SafeAddr(vec) ? vec : 0;
    }

    static uintptr_t GetRuleset() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return 0;
        uintptr_t rs = *(uintptr_t*)(sharedGameManager.instance + 0x3E0);
        return SafeAddr(rs) ? rs : 0;
    }

    static FrictionProperties* GetFriction() {
        uintptr_t tbl = GetTableInst();
        if (!tbl) return nullptr;
        uintptr_t fp = *(uintptr_t*)(tbl + 0x3C0);
        return SafeAddr(fp) ? (FrictionProperties*)fp : nullptr;
    }

    struct BallData { uintptr_t inst; int cls; int state; };

    // Iterasi bola dengan HARD LIMIT dan bounds check ketat
    static int GetAllBalls(BallData out[], int maxOut) {
        uintptr_t tbl = GetTableInst();
        if (!tbl) return 0;
        uintptr_t arrPtr = *(uintptr_t*)(tbl + 0x450);
        if (!SafeAddr(arrPtr)) return 0;
        uintptr_t count = *(uintptr_t*)(arrPtr + 0x08);
        if (count == 0 || count > 16) return 0;   // SAFETY: reject garbage count
        uintptr_t* data = *(uintptr_t**)(arrPtr + 0x18);
        if (!SafeAddr((uintptr_t)data)) return 0;
        int found = 0;
        for (uintptr_t i = 0; i < count && found < maxOut; i++) {
            uintptr_t inst = data[i];
            if (!SafeAddr(inst)) continue;
            int cls   = *(int*)(inst + 0xA0);
            int state = *(int*)(inst + 0xA4);
            if (cls < -1 || cls > 10) continue;   // SAFETY: reject garbage classification
            out[found++] = { inst, cls, state };
        }
        return found;
    }

    static uintptr_t GetCueBall() {
        BallData balls[16];
        int n = GetAllBalls(balls, 16);
        for (int i = 0; i < n; i++)
            if (balls[i].cls == 0) return balls[i].inst;
        return 0;
    }

    static bool IsInGame() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return false;
        uintptr_t sm = *(uintptr_t*)(sharedGameManager.instance + 0x508);
        if (!SafeAddr(sm)) return false;
        // StateStack at +0x08: PNSArray*
        uintptr_t stack = *(uintptr_t*)(sm + 0x08);
        if (!SafeAddr(stack)) return false;
        uintptr_t cnt = *(uintptr_t*)(stack + 0x08);
        if (cnt == 0 || cnt > 20) return false;
        return true;  // GameStateManager exists → in game
    }

    // ════════════════════════════════════════════════════════════════════
    // TICK — dipanggil setiap frame HANYA untuk baca + tulis aman
    // ════════════════════════════════════════════════════════════════════
    static void Tick() {
        // ── Live reads (aman: read-only) ─────────────────────────────────
        uintptr_t vec = GetVECBase();
        if (vec) {
            rdEnglishX = *(double*)(vec + 0x000);
            rdEnglishY = *(double*)(vec + 0x008);
            rdAimOffX  = *(double*)(vec + 0x010);
            rdAimOffY  = *(double*)(vec + 0x018);
        }
        if (SafeAddr(libmain)) {
            rdMaxPowerNow = *(double*)(libmain + 0x4E49410);
            rdMaxSpinNow  = *(double*)(libmain + 0x4E49418);
        }
        uintptr_t vc = GetVCInst();
        if (vc) rdPowerNow = *(double*)(vc + 0x3B0);
        uintptr_t vg = GetVGInst();
        if (vg) rdCurrentAngle = *(double*)(vg + 0x028);
        uintptr_t cue = GetCueBall();
        if (cue) rdCueBallRadius = *(double*)(cue + 0x040);

        // ── [1] mAimOffset — AMAN (VEC adalah UI data) ───────────────────
        if (bAimOffsetActive && vec) {
            *(double*)(vec + 0x010) = (double)fAimOffsetX;
            *(double*)(vec + 0x018) = (double)fAimOffsetY;
        }

        // ── [2] CUE_MAX_POWER — AMAN (.rodata dengan mprotect) ───────────
        if (SafeAddr(libmain)) {
            uintptr_t addrP = libmain + 0x4E49410;
            uintptr_t addrS = libmain + 0x4E49418;
            if (bMaxPowerActive) {
                if (origMaxPower == 0.0) origMaxPower = *(double*)addrP;
                SafeWriteDouble(addrP, origMaxPower * (double)fMaxPowerMul);
            } else if (origMaxPower != 0.0) {
                SafeWriteDouble(addrP, origMaxPower); origMaxPower = 0.0;
            }
            if (bMaxSpinActive) {
                if (origMaxSpin == 0.0) origMaxSpin = *(double*)addrS;
                SafeWriteDouble(addrS, origMaxSpin * (double)fMaxSpinMul);
            } else if (origMaxSpin != 0.0) {
                SafeWriteDouble(addrS, origMaxSpin); origMaxSpin = 0.0;
            }
        }

        // ── [5] Pocket Nomination Bypass — write ke offset verified ──────
        if (bNomBypass) {
            uintptr_t rs = GetRuleset();
            if (rs) *(int*)(rs + 0x068) = 0;
        }

        // ── [6] Golden Shot Mode ──────────────────────────────────────────
        if (bGoldenShot && SafeAddr(sharedGameManager.instance))
            *(int*)(sharedGameManager.instance + 0x5C0) = 16;

        // ── [9] Shot Power Force Max — AMAN (mPower = UI, bukan physics) ─
        if (bShotPowerMax && vc)
            *(double*)(vc + 0x3B0) = 1.0;

        // ── [10] Aim Angle Override — AMAN (mAimAngle = UI state) ────────
        if (bAimAngleOverride && vg) {
            double radians = (double)fAimAngleDeg * (3.14159265358979 / 180.0);
            *(double*)(vg + 0x028) = radians;
        }

        // ── [12] Zero Friction — tulis HANYA saat toggle berubah ─────────
        // CRASH FIX: tidak tulis setiap frame, hanya saat state change
        if (bZeroFriction != bFrictionPrev) {
            bFrictionPrev = bZeroFriction;
            FrictionProperties* fp = GetFriction();
            if (fp) {
                if (bZeroFriction) {
                    // Save originals
                    origFriction[0] = fp->_coefficientOfSlidingFriction;
                    origFriction[1] = fp->_coefficientOfRollingFriction;
                    origFriction[2] = fp->_coefficientOfSpinningFriction;
                    origFriction[3] = fp->_timeOfequilibriumFactor;
                    origFriction[4] = fp->_velocityReductionSlidingFactor;
                    origFriction[5] = fp->_velocityReductionRollingFactor;
                    origFriction[6] = fp->_deltaSpinFactor;
                    bFrictionSaved  = true;
                    // Apply zero friction — skip offset 0x0 untuk aman
                    fp->_coefficientOfRollingFriction    = 0.0002;
                    fp->_coefficientOfSpinningFriction   = 0.0002;
                    fp->_velocityReductionSlidingFactor  = 0.05;
                    fp->_velocityReductionRollingFactor  = 0.05;
                } else if (bFrictionSaved) {
                    fp->_coefficientOfSlidingFriction   = origFriction[0];
                    fp->_coefficientOfRollingFriction    = origFriction[1];
                    fp->_coefficientOfSpinningFriction   = origFriction[2];
                    fp->_timeOfequilibriumFactor         = origFriction[3];
                    fp->_velocityReductionSlidingFactor  = origFriction[4];
                    fp->_velocityReductionRollingFactor  = origFriction[5];
                    fp->_deltaSpinFactor                 = origFriction[6];
                    bFrictionSaved = false;
                }
            }
        }

        // ── [13] Cue Ball Ghost — tulis HANYA saat toggle berubah ────────
        // CRASH FIX: tidak tulis setiap frame
        if (bCueBallGhost != bCueBallGhostPrev) {
            bCueBallGhostPrev = bCueBallGhost;
            uintptr_t cuenow = GetCueBall();
            if (cuenow) {
                if (bCueBallGhost) {
                    origCueBallRadius = *(double*)(cuenow + 0x040);
                    *(double*)(cuenow + 0x040) = origCueBallRadius * (double)fGhostRadius;
                } else if (origCueBallRadius != 0.0) {
                    *(double*)(cuenow + 0x040) = origCueBallRadius;
                    origCueBallRadius = 0.0;
                }
            }
        }

        // ── [16] Ruleset Nomination Override — hanya offset verified ──────
        if (bRuleNomZero) {
            uintptr_t rs = GetRuleset();
            if (rs) *(int*)(rs + 0x068) = 0;
        }

        // ── [20] CCDirector Scan — READ-ONLY, tidak ada write ────────────
        if (bDirScan && sharedDirector && SafeAddr(sharedDirector.instance)) {
            char tmp[384] = "";
            int found = 0;
            for (int off = iDirScanOffset;
                 off < iDirScanOffset + 0x180 && found < 8; off += 4) {
                uintptr_t addr = sharedDirector.instance + off;
                if (!SafeAddr(addr)) continue;
                float fv = *(float*)addr;
                if (fv >= 0.01f && fv <= 100.0f && !std::isnan(fv) && !std::isinf(fv)) {
                    char line[64];
                    snprintf(line, sizeof(line), "+%03X:%.4f  ", off, fv);
                    strncat(tmp, line, sizeof(tmp) - strlen(tmp) - 1);
                    found++;
                }
            }
            snprintf(szDirScan, sizeof(szDirScan), "[DIR=%p]\n%s",
                (void*)sharedDirector.instance, tmp[0] ? tmp : "(no float candidates)");
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // ONE-SHOT ACTIONS — dipanggil hanya dari tombol, bukan Tick()
    // ════════════════════════════════════════════════════════════════════

    // [11] Enemy Ball Sink — ONE-SHOT
    static void DoEnemySink() {
        BallData balls[16];
        int n = GetAllBalls(balls, 16);
        iSinkCount = 0;
        for (int i = 0; i < n; i++) {
            int cls = balls[i].cls;
            if (cls == 0) continue;   // skip cue ball
            if (cls == 4) continue;   // skip 8-ball (too risky)
            if (balls[i].state != 1 && balls[i].state != 2) continue;
            *(int*)(balls[i].inst + 0xA4) = 4;  // POTTED
            iSinkCount++;
        }
        snprintf(szSinkResult, sizeof(szSinkResult),
            "Sink: %d balls forced POTTED", iSinkCount);
    }

    // [14] Cue Ball Warp — ONE-SHOT
    static void DoCueBallWarp() {
        uintptr_t cue = GetCueBall();
        if (!cue) { return; }
        *(double*)(cue + 0x020) = (double)fWarpX;
        *(double*)(cue + 0x028) = (double)fWarpY;
        *(double*)(cue + 0x030) = 0.0;   // stop velocity
        *(double*)(cue + 0x038) = 0.0;
    }

    // [15] Velocity Burst — ONE-SHOT
    static void DoVelocityBurst() {
        BallData balls[16];
        int n = GetAllBalls(balls, 16);
        for (int i = 0; i < n; i++) {
            if (balls[i].cls == 0) continue;
            if (balls[i].state != 1 && balls[i].state != 2) continue;
            float angle = ((float)(rand() % 628)) / 100.0f;
            *(double*)(balls[i].inst + 0x030) = fBurstStrength * cosf(angle);
            *(double*)(balls[i].inst + 0x038) = fBurstStrength * sinf(angle);
        }
    }

    // [17] Coins Write — ONE-SHOT
    static void DoCoinsWrite() {
        if (!sharedUserInfo || !SafeAddr(sharedUserInfo.instance)) {
            snprintf(szCoinsResult, sizeof(szCoinsResult), "ERR: no UserInfo"); return;
        }
        uintptr_t cp = *(uintptr_t*)(sharedUserInfo.instance + 0x208);
        if (!SafeAddr(cp)) {
            snprintf(szCoinsResult, sizeof(szCoinsResult), "ERR: no coins ptr"); return;
        }
        *(double*)(cp + 0x10) = (double)fCoinsValue;
        uintptr_t cashp = *(uintptr_t*)(sharedUserInfo.instance + 0x210);
        if (SafeAddr(cashp)) *(double*)(cashp + 0x10) = 9999.0;
        snprintf(szCoinsResult, sizeof(szCoinsResult), "Written %.0f coins", (double)fCoinsValue);
    }

    // ════════════════════════════════════════════════════════════════════
    // PACKET LOGGER
    // ════════════════════════════════════════════════════════════════════
    static void pktHex(const uint8_t* b, size_t len, char* out, size_t sz) {
        size_t n = len > 20 ? 20 : len, pos = 0;
        for (size_t i = 0; i < n && pos + 4 < sz; i++)
            pos += snprintf(out + pos, sz - pos, "%02X ", b[i]);
        if (len > 20 && pos + 3 < sz) snprintf(out + pos, sz - pos, "...");
    }

    static ssize_t my_send(int fd, const void* buf, size_t len, int flags) {
        ssize_t ret = _origSend(fd, buf, len, flags);
        if (bPacketLogActive && len >= 4 && len < 8192) {
            std::lock_guard<std::mutex> lk(mtxPkt);
            iSendCount.fetch_add(1);
            lastSendLen = len;
            pktHex((const uint8_t*)buf, len, szLastSend, sizeof(szLastSend));
        }
        return ret;
    }

    static ssize_t my_recv(int fd, void* buf, size_t len, int flags) {
        ssize_t ret = _origRecv(fd, buf, len, flags);
        if (bPacketLogActive && ret > 3 && (size_t)ret < 8192) {
            std::lock_guard<std::mutex> lk(mtxPkt);
            iRecvCount.fetch_add(1);
            lastRecvLen = (size_t)ret;
            pktHex((const uint8_t*)buf, (size_t)ret, szLastRecv, sizeof(szLastRecv));
        }
        return ret;
    }

    static void InstallPacketHooks() {
        if (bPacketHookInstalled) return;
        xhook_register(".*/com.miniclip.eightballpool/.*",
            "send", (void*)my_send, (void**)&_origSend);
        xhook_register(".*/com.miniclip.eightballpool/.*",
            "recv", (void*)my_recv, (void**)&_origRecv);
        xhook_refresh(0);
        bPacketHookInstalled = true;
    }

    static void SetPacketLog(bool on) {
        if (on && !bPacketHookInstalled) InstallPacketHooks();
        bPacketLogActive = on;
        if (!on) {
            iSendCount.store(0); iRecvCount.store(0);
            std::lock_guard<std::mutex> lk(mtxPkt);
            strcpy(szLastSend, "—"); strcpy(szLastRecv, "—");
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // NETWORK PROBES (threaded)
    // ════════════════════════════════════════════════════════════════════
    static void RunAPITest(const std::string& userId) {
        if (bAPITestRunning.load()) return;
        bAPITestRunning.store(true);
        snprintf(szAPIResult, sizeof(szAPIResult), "Probing...");
        std::thread([uid = userId]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) { snprintf(szAPIResult, sizeof(szAPIResult), "FAIL: socket()"); bAPITestRunning.store(false); return; }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "3.127.149.181", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                snprintf(szAPIResult, sizeof(szAPIResult), "REFUSED:80 (HTTPS-only)");
                close(sock); bAPITestRunning.store(false); return;
            }
            char req[400];
            snprintf(req, sizeof(req),
                "GET /v1/grant-reward?user_id=%s HTTP/1.1\r\n"
                "Host: prod-pool-reward-links-service.pool.miniclippt.com\r\n"
                "Connection: close\r\n\r\n", uid.c_str());
            send(sock, req, strlen(req), 0);
            char buf[512]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) { buf[n] = '\0'; char* nl = strchr(buf, '\n'); if (nl) *nl = '\0'; snprintf(szAPIResult, sizeof(szAPIResult), "%s", buf); }
            else snprintf(szAPIResult, sizeof(szAPIResult), "No response");
            bAPITestRunning.store(false);
        }).detach();
    }

    static void RunBACONProbe() {
        if (bBACONRunning.load()) return;
        bBACONRunning.store(true);
        snprintf(szBACONResult, sizeof(szBACONResult), "Probing...");
        std::thread([]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) { snprintf(szBACONResult, sizeof(szBACONResult), "FAIL: socket"); bBACONRunning.store(false); return; }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "54.73.12.200", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                sa.sin_port = htons(8080);
                if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                    snprintf(szBACONResult, sizeof(szBACONResult), "REFUSED p80+p8080 — TLS only");
                    close(sock); bBACONRunning.store(false); return;
                }
            }
            const char* wsReq =
                "GET /ws HTTP/1.1\r\nHost: pool-bacon-live.pool.miniclippt.com\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n";
            send(sock, wsReq, strlen(wsReq), 0);
            char buf[400]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) { buf[n] = '\0'; char* nl = strchr(buf, '\n'); if (nl) *nl = '\0'; snprintf(szBACONResult, sizeof(szBACONResult), "%s", buf); }
            else snprintf(szBACONResult, sizeof(szBACONResult), "Connected — no plaintext resp");
            bBACONRunning.store(false);
        }).detach();
    }

    static void RunS3Probe() {
        if (bS3Running.load()) return;
        bS3Running.store(true);
        snprintf(szS3Result, sizeof(szS3Result), "Probing...");
        std::thread([]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) { snprintf(szS3Result, sizeof(szS3Result), "FAIL: socket"); bS3Running.store(false); return; }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "52.217.10.100", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                snprintf(szS3Result, sizeof(szS3Result), "TCP REFUSED:80 — S3 HTTPS-only");
                close(sock); bS3Running.store(false); return;
            }
            const char* req =
                "GET / HTTP/1.1\r\nHost: latest-live-config.pool.miniclippt.com\r\n"
                "Connection: close\r\n\r\n";
            send(sock, req, strlen(req), 0);
            char buf[400]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) { buf[n] = '\0'; char* nl = strchr(buf, '\n'); if (nl) *nl = '\0'; snprintf(szS3Result, sizeof(szS3Result), "%s", buf); }
            else snprintf(szS3Result, sizeof(szS3Result), "No plaintext resp (HTTPS-only)");
            bS3Running.store(false);
        }).detach();
    }

    // ════════════════════════════════════════════════════════════════════
    // RESET ALL
    // ════════════════════════════════════════════════════════════════════
    static void ResetAll() {
        bAimOffsetActive = false; fAimOffsetX = fAimOffsetY = 0.0f;
        bMaxPowerActive = false; fMaxPowerMul = 2.0f;
        bMaxSpinActive  = false; fMaxSpinMul  = 2.0f;
        bVIPActive      = false;
        bNomBypass      = false;
        bGoldenShot     = false;
        SetPacketLog(false);
        bShotPowerMax      = false;
        bAimAngleOverride  = false;
        if (bZeroFriction) { bZeroFriction = false; }   // Tick() will restore
        if (bCueBallGhost) { bCueBallGhost = false; }   // Tick() will restore
        bRuleNomZero = false;
        bDirScan     = false;
        if (origMaxPower != 0.0 && SafeAddr(libmain)) SafeWriteDouble(libmain + 0x4E49410, origMaxPower);
        if (origMaxSpin  != 0.0 && SafeAddr(libmain)) SafeWriteDouble(libmain + 0x4E49418, origMaxSpin);
        origMaxPower = origMaxSpin = 0.0;
    }

} // namespace Experiment
